# Unreal editor bridge on the development PC

Windows remains the game target. This repository pins **ue-mcp 1.3.9**, the version explicitly requested, as development tooling. Unreal Engine is a separate installation. The owner has started the Unreal installation; successful completion and an actual editor/toolchain remain unverified.

Upstream: [v1.3.9 release](https://github.com/db-lyon/ue-mcp/releases/tag/v1.3.9), [installation](https://github.com/db-lyon/ue-mcp/blob/v1.3.9/docs/getting-started.md), [client configuration](https://github.com/db-lyon/ue-mcp/blob/v1.3.9/docs/configuration.md), [MIT license](https://github.com/db-lyon/ue-mcp/blob/v1.3.9/LICENSE).

## Install Unreal on the owner's Windows PC

1. Download the [Epic Games Launcher from the official Unreal page](https://www.unrealengine.com/en-US/download), install it and sign in.
2. Open Unreal Engine → Library, add an engine installation and choose an available stable UE 5 release within the bridge vendor's advertised 5.4–5.8 range. Record the actual version and installation folder; do not choose a preview merely to match an example.
3. Install the C++ game development tools and Windows SDK required by that engine. Follow [Epic's Visual Studio setup documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine) for the selected version.
4. Install [Node.js](https://nodejs.org/) 20+ for the editor bridge. CI verified the bridge with Node 22.23.3 on Windows.
5. Connect an authorized PC terminal integration, or continue in Codex on that PC, so development can inspect files, build native code and run the editor.

A remote computer connection was demonstrated and local source checks passed. The engine installation currently faces a reported storage blocker; its actual requirements and successful completion remain unverified.

## Inspect before creating the game project

1. Check the real engine folder, its Engine/Build/Build.version, Windows editor, Build.bat and RunUAT.bat. Run Scripts/Inspect-Environment.ps1 as described in BUILD.md; keep the report.
2. Verify Visual Studio's C++ tools and Windows SDK against the discovered engine's requirements.
3. Verify Node.js with node --version. Use **Node 20 or newer**: the v1.3.9 package declares this requirement, despite the getting-started page saying 18+.
4. Inspect any existing BLACKGLASS checkout, project and uncommitted changes. Create the native C++ Unreal project only after confirming the installed engine; do not migrate an existing project or assume an engine version.

Upstream advertises Windows UE 5.4–5.8 compatibility. That is a vendor claim until the bridge compiles and connects on the actual selected installation. Epic's wrapped native tools are restricted to UE 5.8+; the game must not rely on their availability.

## Install the pinned bridge after the real .uproject exists

From the development checkout, install the editor tooling:

~~~powershell
npm install --ignore-scripts --no-audit --no-fund
~~~

Close the editor, change to the directory containing the actual .uproject, and run the pinned upstream wizard:

~~~powershell
npx -y ue-mcp@1.3.9 init
~~~

The wizard deploys C++ bridge sources to Plugins/UE_MCP_Bridge and updates the project plugin list, including PythonScriptPlugin and other categories selected during setup. Review the resulting project changes before committing. Keep optional feedback posting, OAuth and global client changes disabled unless separately requested.

Build using the real project path with the editor closed:

~~~powershell
npx -y ue-mcp@1.3.9 build "C:\actual\BLACKGLASS\BLACKGLASS.uproject"
~~~

The path above is an example, not an existing file. Open the resulting project in the verified engine after the build succeeds. Check the bridge log, then verify the MCP handshake. This is an editor-only bridge; it does not ship as a gameplay system.

Do not run init against the current standalone core repository: it does not yet contain a .uproject. Do not run update --build: this project deliberately pins the requested release and preserves the installed engine.

## Connect the local Codex client

ue-mcp uses **local stdin/stdout MCP**, with a loopback editor bridge. It must run on the Windows PC beside the editor. Adding these files to GitHub does not register tools in an existing remote chat.

Merge the example below into that PC's ~/.codex/config.toml, substituting the actual project path and directory while preserving existing entries:

~~~toml
[mcp_servers.ue-mcp]
command = "npx"
args = ["-y", "ue-mcp@1.3.9", "C:/actual/BLACKGLASS/BLACKGLASS.uproject"]
cwd = "C:/actual/BLACKGLASS"
enabled = true
~~~

Restart the client after configuring it. A connected local Codex session can continue implementation in the same repository. Alternatively, connect an authorized Windows terminal/editor integration to the active session so it can inspect and operate this PC.

Call project(action="get_status") and record the actual response. Connection is accepted only when editorConnected is true, editorTarget.projectPath matches BLACKGLASS, and the deployedPlugin/bridgeProtocol report is consistent with the compiled release. A running npm process alone does not establish editor access.

The no-project mode can connect to any bridge answering legacy port 9877. Use the explicit BLACKGLASS project argument for actual development. The optional upstream HTTP endpoint serves flows, not a general remote MCP endpoint.

## Remote Desktop Commander setup reference

The [upstream Remote Device guide](https://github.com/wonderwhy-er/DesktopCommanderMCP/blob/main/src/remote-device/README.md) describes the local connection process:

~~~sh
npx @wonderwhy-er/desktop-commander@latest remote
~~~

Complete the browser authorization, keep the process running, and follow the AI-client connection instructions at [mcp.desktopcommander.app](https://mcp.desktopcommander.app). Installing the ChatGPT plugin alone does not start that local process.

On macOS, UI automation and screen capture may require OS permissions for the application hosting the process. Verify the responsible application before changing permissions. These are generic setup instructions; detailed private machine state is not published.

## What CI proves

.github/workflows/ue-mcp-validation.yml installs the pinned published npm package on an isolated GitHub Windows VM, starts its real process, performs MCP initialization/tool discovery and calls project(get_status). The probe refuses to run if legacy port 9877 is already active.

Artifacts/UnrealBridge/mcp-probe.json records the actual server version, advertised tool names and disconnected status. The artifact also retains the generated package-lock.json for the dependencies resolved during that run; only ue-mcp's direct version is currently committed as a pin.

[Windows run 36922455754](https://github.com/Velamj/BLACKGLASS/actions/runs/36922455754), source a19b4317748d07aee061bd79dacfaa1a220bc209, passed all four checks on 2026-10-01: pinned version, real MCP initialization, tool discovery (27 advertised tools) and truthful disconnected status. Artifact unreal-mcp-windows-probe (11192232303) contains the report and resolved dependency lock. The pull request check passed as well.

This check does not compile the C++ Unreal plugin, connect an editor, create a game, measure frame rate or validate Windows game packaging. Those require the owner's installed engine and ordinary-controls playtests. The separate core and engine checks remain authoritative for their own scope.
