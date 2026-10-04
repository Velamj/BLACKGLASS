"""Profile the real packaged foundation; discard boot capture and keep raw data local."""
from __future__ import annotations

import argparse
import csv
import ctypes
from ctypes import wintypes
import datetime as dt
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time

QUERY_KEYS = (
    "r.ScreenPercentage", "r.ShadowQuality", "r.AntiAliasingMethod", "r.VSync",
    "t.MaxFPS", "sg.ViewDistanceQuality", "sg.EffectsQuality", "sg.TextureQuality",
    "sg.PostProcessQuality", "r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod",
    "r.Shadow.Virtual.Enable",
)
REQUIRED_KEYS = QUERY_KEYS[:7]
WORLD_PATH = "/Game/Maps/DepotBlock.DepotBlock:PersistentLevel."


def powershell_json(script: str):
    result = subprocess.run(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=20, check=False,
    )
    if result.returncode:
        raise ValueError("A scoped Windows inspection failed.")
    return json.loads(result.stdout)


def inspect_hardware() -> dict:
    # Select only public hardware specifications, never names, identifiers or serials.
    return powershell_json("""
$c = Get-CimInstance Win32_Processor | Select-Object -First 1
$o = Get-CimInstance Win32_OperatingSystem
$g = Get-CimInstance Win32_VideoController | Select-Object -First 1
[ordered]@{
 cpu=$c.Name; physical_cores=[int]$c.NumberOfCores
 logical_processors=[int]$c.NumberOfLogicalProcessors
 memory_gib=[math]::Round($o.TotalVisibleMemorySize/1048576,2)
 gpu=$g.Name; gpu_driver=$g.DriverVersion; os=$o.Caption; os_version=$o.Version
} | ConvertTo-Json -Compress
""")


def other_instances() -> int:
    return int(powershell_json("""
@(Get-Process -Name Blackglass,UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count |
 ConvertTo-Json -Compress
"""))


def client_size(pid: int) -> tuple[int, int] | None:
    user = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = (callback_type, wintypes.LPARAM)
    user.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
    user.GetClientRect.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.RECT))
    user.IsWindowVisible.argtypes = (wintypes.HWND,)
    user.GetWindow.argtypes = (wintypes.HWND, wintypes.UINT)
    user.GetWindow.restype = wintypes.HWND
    user.GetAncestor.argtypes = (wintypes.HWND, wintypes.UINT)
    user.GetAncestor.restype = wintypes.HWND
    user.GetClassNameW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
    user.GetClassNameW.restype = ctypes.c_int
    user.EnumWindows.restype = wintypes.BOOL
    user.SetThreadDpiAwarenessContext.argtypes = (ctypes.c_void_p,)
    user.SetThreadDpiAwarenessContext.restype = ctypes.c_void_p
    matches = []

    @callback_type
    def visit(window, unused):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(window, ctypes.byref(owner))
        rectangle = wintypes.RECT()
        window_class = ctypes.create_unicode_buffer(128)
        user.GetClassNameW(window, window_class, len(window_class))
        if (owner.value == pid and user.IsWindowVisible(window)
                and not user.GetWindow(window, 4) and user.GetAncestor(window, 2) == window
                and window_class.value == "UnrealWindow"
                and user.GetClientRect(window, ctypes.byref(rectangle))):
            width, height = rectangle.right - rectangle.left, rectangle.bottom - rectangle.top
            if width > 100 and height > 100:
                matches.append((width, height))
        return True

    # DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 prevents logical-pixel virtualization.
    # Scope it to this measurement thread and restore its original awareness afterwards.
    previous = user.SetThreadDpiAwarenessContext(ctypes.c_void_p(-4))
    if not previous:
        raise OSError(ctypes.get_last_error(), "Physical game-client measurement could not set thread DPI awareness.")
    try:
        ctypes.set_last_error(0)
        if not user.EnumWindows(visit, 0):
            raise OSError(ctypes.get_last_error(), "Physical game-client window enumeration failed.")
    finally:
        if not user.SetThreadDpiAwarenessContext(previous):
            raise OSError(ctypes.get_last_error(), "Measurement thread DPI awareness could not be restored.")
    if len(matches) > 1:
        raise OSError("Physical game-client window selection was ambiguous.")
    return matches[0] if matches else None


def load_analyzer():
    path = Path(__file__).with_name("Analyze-Performance.py")
    spec = importlib.util.spec_from_file_location("blackglass_performance_analysis", path)
    if spec is None or spec.loader is None:
        raise ValueError("The CSV analysis script is missing.")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def runtime_context(log: str) -> tuple[dict, dict, int | None]:
    quality = {}
    for key in QUERY_KEYS:
        matches = re.findall(re.escape(key) + r'\s*=\s*"([^"]+)"', log)
        if matches:
            try:
                value = float(matches[-1])
                if math.isfinite(value):
                    quality[key] = {"value": value, "source": "runtime_query"}
            except ValueError:
                pass
    known = {}
    for name, class_name in (("units", "BGUnit"), ("vehicles", "BGVehicle"), ("doors", "BGDoor")):
        pattern = r'\d+\)\s+' + class_name + r'\s+([^\r\n]+?)\.EntityID\s*=\s*(-?\d+)'
        ids = {
            int(entity) for path, entity in re.findall(pattern, log, re.IGNORECASE)
            if WORLD_PATH in path
        }
        if ids:
            known[name] = len(ids)
    actor_names = {
        name for name in re.findall(r'\d+\)\s+\S+\s+([^\r\n]+?)\.bHidden\s*=', log)
        if WORLD_PATH in name
    }
    return quality, known, len(actor_names) if actor_names else None


def csv_paths(log: str, executable: Path, saved_root: Path) -> list[Path]:
    result = []
    for raw in re.findall(r"Capture Ended\. Writing CSV to file\s*:\s*([^\r\n]+)", log):
        path = Path(raw.strip().strip('"'))
        path = (executable.parent / path).resolve() if not path.is_absolute() else path.resolve()
        if not path.is_relative_to(saved_root.resolve()):
            raise ValueError("The capture path is outside the packaged Saved directory.")
        if path not in result:
            result.append(path)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--project-root", type=Path, default=root)
    parser.add_argument("--executable", type=Path,
                        help="Actual Artifacts/Windows/BLACKGLASS/Binaries/Win64/Blackglass.exe, not the bootstrap.")
    parser.add_argument("--frames", type=int, default=10000)
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--min-duration", type=float, default=30.0)
    parser.add_argument("--run-label", default="foundation-profile")
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("Profiling requires the actual Windows package and graphics device.")
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", args.run_label):
        parser.error("--run-label must be a simple name")
    if not 2000 <= args.frames <= 1000000 or not 30 <= args.timeout <= 1800:
        parser.error("--frames or --timeout is outside its supported bound")
    if not math.isfinite(args.min_duration) or args.min_duration < 30:
        parser.error("--min-duration must be finite and at least 30 seconds")
    project_root = args.project_root.resolve()
    executable = (args.executable or project_root / "Artifacts/Windows/BLACKGLASS/Binaries/Win64/Blackglass.exe").resolve()
    version_path = args.engine_root / "Engine/Build/Build.version"
    project_path = project_root / "BLACKGLASS.uproject"
    if not executable.is_file() or not version_path.is_file() or not project_path.is_file():
        parser.error("The real package, verified engine installation or project is missing")
    if (executable.name.lower() != "blackglass.exe" or executable.parent.name != "Win64"
            or executable.parent.parent.name != "Binaries" or executable.parents[2].name != "BLACKGLASS"):
        parser.error("--executable must name this project's actual packaged Win64 game")
    archive_root = executable.parents[3]
    packaged_saved = archive_root / "BLACKGLASS/Saved"
    engine = json.loads(version_path.read_text(encoding="utf-8-sig"))
    project = json.loads(project_path.read_text(encoding="utf-8-sig"))
    if project.get("EngineAssociation") != f"{engine['MajorVersion']}.{engine['MinorVersion']}":
        parser.error("Project association and the inspected engine installation differ")
    if args.validate_only:
        print("Profiling inputs validated; no game was launched.")
        return 0
    try:
        if other_instances():
            parser.error("Close other BLACKGLASS and Unreal Editor processes before an isolated capture")
        hardware = inspect_hardware()
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.error(f"Windows prerequisite inspection failed ({type(error).__name__})")

    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_name = f"{args.run_label}-{stamp}"
    local = project_root / "Saved/Verification"
    local.mkdir(parents=True, exist_ok=True)
    log_path = local / f"{run_name}.engine.log"
    output_path = local / f"{run_name}.stdout.log"
    summary_path = local / f"{run_name}.summary.json"
    queries = (*QUERY_KEYS, "getall BGUnit EntityId", "getall BGVehicle EntityId",
               "getall BGDoor EntityId", "getall Actor bHidden")
    command = [
        str(executable), "-windowed", "-ResX=1920", "-ResY=1080", "-ForceRes", "-NoSplash",
        f"-csvCaptureFrames={args.frames}", "-csvRepeat=2",
        # Installed UE CSV worker crashes in allocator TLS cleanup on this host.
        # Use its supported synchronous profiling path; game logic/rendering stay threaded.
        "-csvNoProcessingThread", "-ExitAfterCsvProfiling", "-csvCompression=0", "-csvGpuStats",
        f"-ExecCmds={','.join(queries)}", f"-abslog={log_path}",
    ]
    process = None
    process_exit = None
    error = ""
    error_detail = None
    operation = "launch"
    observed_size = None
    launched = time.monotonic()
    try:
        with output_path.open("wb") as output:
            process = subprocess.Popen(
                command, cwd=archive_root, stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP,
            )
            print(f"Actual packaged profiling process started (PID {process.pid}); keep its game window open.", flush=True)
            while process.poll() is None:
                operation = "measure_physical_client"
                size = client_size(process.pid)
                if size:
                    observed_size = size
                if time.monotonic() - launched > args.timeout:
                    process.kill()  # Only the process launched by this runner.
                    process.wait(timeout=10)
                    error = "The owned profiling process exceeded its limit and was terminated."
                    break
                time.sleep(0.5)
            process_exit = process.returncode
    except Exception as exception:
        # Retain safe API diagnostics without copying filenames, command lines or tokens.
        error_detail = {"operation": operation, "exception_type": type(exception).__name__,
                        "errno": getattr(exception, "errno", None),
                        "winerror": getattr(exception, "winerror", None)}
        if isinstance(exception, OSError):
            message = exception.strerror or (exception.args[0] if exception.args else "")
            if isinstance(message, str) and message.startswith((
                    "Owned game-window", "The exact owned process",
                    "Normal close could not", "Physical game-client", "Measurement thread")):
                error_detail["message"] = message
        error = f"The owned profiling process could not complete during {operation} ({type(exception).__name__})."
    finally:
        # Any parser/inspection error or interruption must release the owned process.
        if process is not None and process.poll() is None:
            try:
                process.kill()
                process.wait(timeout=10)
            except (OSError, subprocess.TimeoutExpired) as exception:
                error = f"Owned-process cleanup failed ({type(exception).__name__})."
        if process is not None:
            process_exit = process.returncode

    measured = warmup = context = None
    csv_count = 0
    try:
        if error or process_exit != 0:
            raise ValueError("The actual profiling process did not exit successfully.")
        if observed_size != (1920, 1080):
            raise ValueError("The live game client was not verified at 1920 by 1080.")
        log = log_path.read_text(encoding="utf-8-sig", errors="replace")
        captures = csv_paths(log, executable, packaged_saved)
        csv_count = len(captures)
        if csv_count != 2:
            raise ValueError("Two completed captures were not recorded; an early clean exit is insufficient.")
        analyzer = load_analyzer()
        warmup, measured = [analyzer.read_capture(path) for path in captures]
        nominal_frames = [int(value) for value in re.findall(r"LogCsvProfiler: Display:\s+Frames\s*:\s*(\d+)", log)]
        if nominal_frames != [args.frames, args.frames]:
            raise ValueError("The engine did not confirm both requested nominal capture lengths.")
        # UE drains render-thread statistics for one extra frame after its nominal end.
        # Report every positive frame sample rather than pretending the drain row is absent.
        if measured["frame_count"] < args.frames - 1 or measured["interior_invalid_frame_rows"]:
            raise ValueError("The measured capture has missing interior or insufficient positive frame samples.")
        if measured["duration_seconds"] < args.min_duration:
            raise ValueError("The actual second capture is shorter than the required measurement duration.")
        quality, known, startup_count = runtime_context(log)
        if any(key not in quality for key in REQUIRED_KEYS):
            raise ValueError("One or more required applied quality queries did not produce a runtime value.")
        counts = measured["observed_actor_counts"].get("ActorCount/TotalActorCount")
        total = counts["minimum"] if counts and counts["minimum"] == counts["maximum"] else None
        build = re.findall(r'Metadata set : config="([^"]+)"', log)
        context = analyzer.selected_context({
            "engine_version": ".".join(str(engine[key]) for key in ("MajorVersion", "MinorVersion", "PatchVersion")),
            "build_configuration": build[-1] if build else None,
            "scenario": "Unpaused DepotBlock with natural NPC simulation and no player orders; second startup capture after discarding the first.",
            "hardware": hardware,
            "resolution": {"width": 1920, "height": 1080, "mode": "windowed", "source": "measured_physical_game_client"},
            "quality": quality, "known_gameplay_actor_counts": known,
            "total_actor_count": total, "warmup_seconds": warmup["duration_seconds"],
        })
        context["startup_persistent_level_actor_count"] = startup_count
        (local / f"{run_name}.context.json").write_text(json.dumps(context, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, KeyError, TypeError, csv.Error) as exception:
        if not error:
            error = str(exception) if isinstance(exception, ValueError) else f"Capture analysis failed ({type(exception).__name__})."

    summary = {
        "schema_version": 1, "passed": not error and measured is not None,
        "process_exit": process_exit, "completed_capture_count": csv_count,
        "observed_physical_game_client": None if observed_size is None else {
            "width": observed_size[0], "height": observed_size[1],
            "source": "scoped_per_monitor_v2_GetClientRect",
        },
        "error": error, "error_detail": error_detail,
        "context": context, "capture": measured,
        "profile_method": {"frames_per_capture": args.frames, "total_captures": 2,
                           "close_method": "supported_ExitAfterCsvProfiling_after_full_capture_drain",
                           "discarded_capture": 1, "gpu_csv_stats_enabled": True,
                           "csv_processing": "synchronous_supported_csvNoProcessingThread",
                           "minimum_measured_duration_seconds": args.min_duration},
        "limitations": [
            "A bounded idle foundation district, not a full mission, combat or final campaign stress test.",
            "CSV data processing runs on the game thread to avoid the observed UE background-worker teardown crash; profiling overhead is included.",
            "Profiling GPU scopes are enabled; global thread times include waits.",
            "Zero or absent GPU timing is unavailable, not proof of zero GPU cost.",
            "The GPU name is OS-reported; a memory amount in that label is not a measured dedicated VRAM capacity.",
            "Startup actor queries count the persistent level, not visible or rendered actors.",
            "Private engine logs and raw CSV metadata remain local and ignored.",
        ],
    }
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    print(f"Local summary: Saved/Verification/{run_name}.summary.json")
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
