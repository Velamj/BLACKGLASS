"""Run actual native BLACKGLASS gameplay automation; never infer success from UBT."""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, required=True)
    parser.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1] / "BLACKGLASS.uproject")
    parser.add_argument("--run-label", default="Foundation")
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", args.run_label):
        parser.error("--run-label must be a simple name, without directory separators")
    if args.timeout < 30 or args.timeout > 1800:
        parser.error("--timeout must be between 30 and 1800 seconds")
    project = args.project.resolve()
    executable = args.engine_root / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
    build_version = args.engine_root / "Engine/Build/Build.version"
    if not project.is_file() or not executable.is_file() or not build_version.is_file():
        parser.error("The specified Unreal installation or project is missing")
    engine = json.loads(build_version.read_text(encoding="utf-8-sig"))
    settings = json.loads(project.read_text(encoding="utf-8-sig"))
    expected = f"{engine['MajorVersion']}.{engine['MinorVersion']}"
    if settings.get("EngineAssociation") != expected:
        parser.error("The project engine association does not match the supplied installation")
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_name = f"{args.run_label}-{stamp}"
    report_dir = project.parent / "Saved/Automation" / run_name
    verification = project.parent / "Saved/Verification"
    report_dir.mkdir(parents=True, exist_ok=False)
    verification.mkdir(parents=True, exist_ok=True)
    stdout_path = verification / f"{run_name}.stdout.log"
    engine_log = verification / f"{run_name}.engine.log"
    summary_path = verification / f"{run_name}.summary.json"
    test_name = "Blackglass.Foundation.Runtime"
    command = [
        str(executable), str(project), "/Game/Maps/DepotBlock",
        "-game", "-unattended", "-NullRHI", "-NoSound", "-NoSplash",
        "-stdout", "-FullStdOutLogOutput",
        f"-ExecCmds=Automation RunTests {test_name}",
        "-TestExit=Automation Test Queue Empty",
        f"-ReportExportPath={report_dir}", f"-abslog={engine_log}",
    ]
    environment = os.environ.copy()
    if not environment.get("TMP") and environment.get("TEMP"):
        environment["TMP"] = environment["TEMP"]
    if not environment.get("ComSpec"):
        environment["ComSpec"] = str(Path(environment.get("SystemRoot", r"C:\Windows")) / "System32/cmd.exe")
    process_exit = None
    launch_error = ""
    save_root = project.parent / "Saved/SaveGames"
    backup_root = verification / f"{run_name}.save-backup"
    backup_root.mkdir()
    autosaves = {}
    # Actual completion/failure invokes the real Autosave path. Preserve player files.
    for name in ("Autosave.sav", "Autosave.sav.previous"):
        source = save_root / name
        backup = backup_root / name if source.is_file() else None
        if backup is not None:
            shutil.copy2(source, backup)
        autosaves[name] = backup
    try:
        with stdout_path.open("wb") as output:
            process_exit = subprocess.run(
                command, cwd=project.parent, env=environment, stdout=output,
                stderr=subprocess.STDOUT, timeout=args.timeout, check=False,
            ).returncode
    except subprocess.TimeoutExpired:
        launch_error = "The native runtime process exceeded its configured time limit and was terminated."
    except OSError as error:
        launch_error = f"The native runtime process could not launch (OS error {error.errno})."
    finally:
        for name, backup in autosaves.items():
            destination = save_root / name
            try:
                if backup is None:
                    destination.unlink(missing_ok=True)
                else:
                    save_root.mkdir(parents=True, exist_ok=True)
                    temporary = save_root / f"{name}.test-restore-{run_name}"
                    shutil.copy2(backup, temporary)
                    os.replace(temporary, destination)
            except OSError as error:
                launch_error = f"Autosave restoration failed (OS error {error.errno}); the original backup was retained."
    report = None
    report_error = ""
    try:
        report = json.loads((report_dir / "index.json").read_text(encoding="utf-8-sig"))
    except (OSError, ValueError):
        report_error = "The engine did not produce a readable Automation index.json report."
    tests = [] if report is None else [
        entry for entry in report.get("tests", []) if entry.get("fullTestPath") == test_name
    ]
    selected = tests[0] if len(tests) == 1 else {}
    entries = selected.get("entries", [])
    errors = [
        entry.get("event", {}).get("message", "")
        for entry in entries if entry.get("event", {}).get("type") == "Error"
    ]
    evidence = [
        entry.get("event", {}).get("message", "")
        for entry in entries
        if entry.get("event", {}).get("type") == "Info"
        and not entry.get("event", {}).get("context")
    ]
    # TestExit can return zero even for a failed suite. The actual engine report is required.
    passed = (
        process_exit == 0 and not launch_error and report is not None and len(tests) == 1
        and selected.get("state") == "Success" and report.get("failed") == 0
        and report.get("notRun") == 0 and report.get("inProcess") == 0
        and report.get("succeeded", 0) + report.get("succeededWithWarnings", 0) == 1
        and not errors
    )
    summary = {
        "test": test_name,
        "engine_version": ".".join(str(engine[key]) for key in ("MajorVersion", "MinorVersion", "PatchVersion")),
        "passed": passed,
        "process_exit": process_exit,
        "automation_state": selected.get("state", "Unavailable"),
        "duration_seconds": selected.get("duration"),
        "failed_tests": None if report is None else report.get("failed"),
        "errors": errors,
        "evidence": evidence,
        "launch_error": launch_error,
        "report_error": report_error,
        "limitations": "Headless runtime systems only; this is not rendering, performance, manual-input or packaged-build evidence.",
    }
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    print(f"Local report: Saved/Automation/{run_name}/index.json")
    print(f"Local summary: Saved/Verification/{run_name}.summary.json")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())