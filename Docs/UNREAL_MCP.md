# Unreal editor bridge

BLACKGLASS contains a native Unreal project, BLACKGLASS.uproject, targeting UE 5.8. The development dependency and vendored editor bridge are pinned to **ue-mcp 1.3.9**. The native editor bridge compiled, and an actual local MCP SDK handshake passed at the recorded checkpoint. The one-shot client exited and the editor was subsequently closed cleanly. This verifies the tested editor connection; gameplay, performance and packaging have their own evidence in [VERIFICATION.md](VERIFICATION.md).

Upstream: [release](https://github.com/db-lyon/ue-mcp/releases/tag/v1.3.9), [installation](https://github.com/db-lyon/ue-mcp/blob/v1.3.9/docs/getting-started.md), [configuration](https://github.com/db-lyon/ue-mcp/blob/v1.3.9/docs/configuration.md), [MIT license](https://github.com/db-lyon/ue-mcp/blob/v1.3.9/LICENSE).

## Prerequisites

Inspect the actual installed engine, compiler and Windows SDK before building; follow [BUILD.md](BUILD.md) and [Epic's version-specific Visual Studio requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.8). Preserve existing projects and engine installations.

The pinned npm package requires **Node 20 or newer**, despite the upstream getting-started page saying 18+. The vendor advertises Windows UE 5.4–5.8 support; compatibility with this project requires an actual build and handshake. The wrapper for Epic native tools additionally requires UE 5.8+ and an available editor registry.

## Deployed project files

- Plugins/UE_MCP_Bridge: vendored C++ editor modules UE_MCP_BridgeStatus and UE_MCP_Bridge. All 264 vendor files matched the installed 1.3.9 package during the deployment audit.
- BLACKGLASS.uproject: PythonScriptPlugin, EditorScriptingUtilities and UE_MCP_Bridge enabled alongside the Blackglass runtime module.
- ue-mcp.yml: project-local bridge settings, including feedback disabled.
- Plugins/UE_MCP_Bridge/LICENSE: complete MIT notice retained with the vendored sources; original package notice is node_modules/ue-mcp/LICENSE.

The bridge is editor tooling, not a gameplay runtime module. Generated plugin binaries and intermediate files are ignored. The asset manifest records provenance and license requirements.

## Verified bridge result

A read-only MCP SDK probe against the actual BLACKGLASS editor passed on 2026-10-03:

- Pinned server version matched: true.
- Editor connected and live: true.
- Bound project and responding editor matched BLACKGLASS: true.
- Plugin/client bridge protocol matched version 2: true.
- Compiled plugin stale or advertised methods missing: false.
- Feedback tool exposed: false.

The responding socket was verified as belonging to the full editor process. The separate standalone game was untouched. Raw responses remain in ignored local reports because they contain private machine paths. The connection probe itself performed no PIE, Python execution or world editing. Optional native-tool wrappers were not exercised.

A subsequent test exercised the actual native editor actions:

- editor(play_in_editor, pieAction=start) changed state; a separate status confirmed isPlaying=true.
- editor(capture_screenshot, target=pie) wrote a real 1280 × 723 DepotBlock game-viewport PNG with the HUD.
- editor(play_in_editor, pieAction=stop) changed state; a separate status confirmed isPlaying=false.

The inspected, byte-identical game-viewport capture is retained as [mcp-pie.png](Evidence/mcp-pie.png); its source and hash are recorded in ASSET_MANIFEST.md. The source capture and raw action report remain in ignored local folders. No Python, editor content changes or persistent mission commands were used. This establishes the tested PIE controls and screenshot action; it does not establish ordinary-input mission completion, performance or packaging.

## Repeat source deployment only when needed

With the editor closed, use the BLACKGLASS checkout as the working directory. Restore the pinned dependency with npm install --ignore-scripts --no-audit --no-fund. For a noninteractive terminal, the verified vendor deployment entry point is:

~~~powershell
node .\node_modules\ue-mcp\dist\deploy-cli.js .\BLACKGLASS.uproject
~~~

The vendor's interactive init wizard needs a real TTY; it calls stdin.setRawMode and cannot run through an ordinary piped remote terminal. For an interactive local terminal, the supported command is npx -y ue-mcp@1.3.9 init .\BLACKGLASS.uproject. Review any wizard-created configuration, optional hooks and client changes. Do not rerun deployment during a build or against an unrelated project.

The existing ue-mcp.yml contains:

~~~yaml
ue-mcp:
  version: 1
  contentRoots:
    - /Game/
  disable:
    - feedback
  nativeTools:
    enabled: true
  context:
    strategy: full

tasks: {}
flows: {}
~~~

Feedback/OAuth posting is disabled in project configuration. nativeTools.enabled requests the optional wrapper; it does not prove those tools are available. No global client configuration or editor connection is implied by these files. Build the native editor target following BUILD.md before opening the project.

## Connect a local MCP client

ue-mcp uses stdin/stdout MCP and a loopback editor bridge. Its process must run beside the editor on the development PC. Publishing source to GitHub does not add tools to an existing remote chat.

For a local Codex client, merge the following example into its configuration, substitute the real checkout path, and preserve existing server entries:

~~~toml
[mcp_servers.ue-mcp]
command = "npx"
args = ["-y", "ue-mcp@1.3.9", "C:/path/to/BLACKGLASS/BLACKGLASS.uproject"]
cwd = "C:/path/to/BLACKGLASS"
enabled = true
~~~

These are example paths, not machine metadata. Restart the client after configuring it, open the correct BLACKGLASS project in the verified engine, and inspect the real bridge log.

Call project(action="get_status"). A connection requires editorConnected=true, editorTarget.projectPath resolving to BLACKGLASS, and a deployedPlugin/bridgeProtocol report consistent with the compiled pinned release. Record the actual response privately when it contains local paths; publish only the project-level result. A running npm server is insufficient.

Use the explicit project argument so the client derives the correct project-specific bridge port. Do not rely on no-project discovery through legacy port 9877 or expose the loopback bridge publicly. The upstream optional HTTP flow endpoint is not a general remote MCP endpoint.

## Scope of existing CI evidence

.github/workflows/ue-mcp-validation.yml installs the pinned published package on an isolated Windows VM, performs MCP initialization/tool discovery and calls project(get_status). [Run 36922455754](https://github.com/Velamj/BLACKGLASS/actions/runs/36922455754) passed the pinned-version, server initialization, advertised-tool discovery and disconnected-status checks. Its artifact records the resolved dependencies and probe response.

That server probe does not compile the vendored C++ plugin, connect a real editor, demonstrate controls or measure a game build. Report editor compilation, handshake, gameplay, performance and packaging separately with actual evidence.
