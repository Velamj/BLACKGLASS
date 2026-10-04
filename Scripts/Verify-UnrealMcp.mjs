// Tests the published MCP process without an Unreal editor. Never deploys a plugin.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { readFile, mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import readline from 'node:readline';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const reportDir = path.join(root, 'Artifacts', 'UnrealBridge');
const expectedVersion = '1.3.9';
const report = {
  scope: 'Published MCP server startup and disconnected status only',
  nodeVersion: process.version,
  platform: process.platform,
  expectedVersion,
  editorVerified: false,
  gameplayVerified: false,
  packagingVerified: false,
  checks: [],
};
let child;
let scratch;
let lines;
let fatal;
let nextId = 1;
const pending = new Map();

function failPending(error) {
  fatal ??= error;
  for (const request of pending.values()) {
    clearTimeout(request.timer);
    request.reject(error);
  }
  pending.clear();
}

function request(method, params) {
  if (fatal) return Promise.reject(fatal);
  const id = nextId++;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pending.delete(id);
      reject(new Error('MCP request timed out: ' + method));
    }, 30000);
    pending.set(id, { resolve, reject, timer });
    child.stdin.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
  });
}

// Projectless ue-mcp can attach to whatever answers port 9877.
// Refuse to start this offline test when that port is already serving anything.
async function requireUnusedBridgePort() {
  await new Promise((resolve, reject) => {
    const socket = net.connect({ host: '127.0.0.1', port: 9877 });
    socket.setTimeout(2000);
    socket.once('connect', () => {
      socket.destroy();
      reject(new Error('Port 9877 is active; run this offline probe on an isolated CI runner.'));
    });
    socket.once('error', (error) => {
      socket.destroy();
      if (error.code === 'ECONNREFUSED') resolve();
      else reject(error);
    });
    socket.once('timeout', () => {
      socket.destroy();
      reject(new Error('Cannot establish that port 9877 is unused.'));
    });
  });
}

try {
  assert.ok(Number(process.versions.node.split('.')[0]) >= 20, 'Node.js 20+ is required');
  const pkgDir = path.join(root, 'node_modules', 'ue-mcp');
  const pkg = JSON.parse(await readFile(path.join(pkgDir, 'package.json'), 'utf8'));
  assert.equal(pkg.version, expectedVersion, 'Installed ue-mcp must match the pinned version');
  report.installedVersion = pkg.version;
  report.checks.push('Pinned package version');

  await requireUnusedBridgePort();
  scratch = await mkdtemp(path.join(os.tmpdir(), 'blackglass-mcp-probe-'));
  child = spawn(process.execPath, [path.join(pkgDir, 'dist', 'index.js')], {
    cwd: scratch,
    stdio: ['pipe', 'pipe', 'pipe'],
    windowsHide: true,
  });
  // Consume diagnostics without publishing machine paths or environment variables.
  child.stderr.resume();
  child.stdin.on('error', failPending);
  child.once('error', failPending);
  child.once('exit', (code) => failPending(new Error('MCP process exited with code ' + code)));
  lines = readline.createInterface({ input: child.stdout });
  lines.on('line', (line) => {
    let message;
    try { message = JSON.parse(line); }
    catch { failPending(new Error('MCP stdout contained a non-JSON protocol line')); return; }
    const entry = pending.get(message.id);
    if (!entry) return; // Server notifications need no response for these read-only calls.
    clearTimeout(entry.timer);
    pending.delete(message.id);
    if (message.error) entry.reject(new Error(JSON.stringify(message.error)));
    else entry.resolve(message.result);
  });

  const initialized = await request('initialize', {
    protocolVersion: '2024-11-05',
    capabilities: {},
    clientInfo: { name: 'blackglass-ci-probe', version: '0.1.0' },
  });
  assert.equal(initialized.serverInfo.name, 'ue-mcp');
  assert.equal(initialized.serverInfo.version, expectedVersion);
  report.serverInfo = initialized.serverInfo;
  report.protocolVersion = initialized.protocolVersion;
  child.stdin.write(JSON.stringify({ jsonrpc: '2.0', method: 'notifications/initialized' }) + '\n');
  report.checks.push('Real stdio MCP initialization');

  const listing = await request('tools/list', {});
  report.tools = listing.tools.map((tool) => tool.name).sort();
  assert.ok(report.tools.includes('project'), 'The project tool must be advertised');
  report.checks.push('Real tool discovery');

  const result = await request('tools/call', {
    name: 'project', arguments: { action: 'get_status' },
  });
  assert.ok(!result.isError, 'project(get_status) must succeed without an editor');
  const status = result.content
    .filter((block) => block.type === 'text')
    .map((block) => { try { return JSON.parse(block.text); } catch { return null; } })
    .find((value) => value && typeof value.editorConnected === 'boolean');
  assert.ok(status, 'Status must explicitly report editor connectivity');
  assert.equal(status.editorConnected, false, 'The offline probe must remain disconnected');
  assert.equal(status.project, null, 'No Unreal project should be bound during this probe');
  assert.equal(status.mode, 'disconnected');
  report.status = status;
  report.checks.push('Truthful disconnected project status');
  report.result = 'passed';
} catch (error) {
  report.result = 'failed';
  report.error = error.message;
  process.exitCode = 1;
} finally {
  if (child && child.exitCode === null && child.signalCode === null) {
    await new Promise((resolve) => {
      const timeout = setTimeout(() => { child.kill('SIGKILL'); resolve(); }, 5000);
      child.once('exit', () => { clearTimeout(timeout); resolve(); });
      child.kill('SIGTERM');
    });
  }
  lines?.close();
  if (scratch) await rm(scratch, { recursive: true, force: true });
  await mkdir(reportDir, { recursive: true });
  await writeFile(path.join(reportDir, 'mcp-probe.json'), JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify(report, null, 2));
}
