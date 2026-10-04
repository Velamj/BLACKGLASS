"""Summarize a real Unreal CSV capture; discard its private footer metadata."""
from __future__ import annotations

import argparse
import csv
import gzip
import json
import math
from pathlib import Path
import statistics

TIMINGS = {
    "frame_time_ms": "FrameTime",
    "game_thread_time_ms": "GameThreadTime",
    "render_thread_time_ms": "RenderThreadTime",
    "gpu_time_ms": "GPUTime",
}
QUALITY_KEYS = (
    "r.ScreenPercentage", "r.ShadowQuality", "r.AntiAliasingMethod", "r.VSync",
    "t.MaxFPS", "sg.ViewDistanceQuality", "sg.EffectsQuality", "sg.TextureQuality",
    "sg.PostProcessQuality", "r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod",
    "r.Shadow.Virtual.Enable",
)


def positive_number(value: str) -> float | None:
    try:
        number = float(value)
    except (ValueError, TypeError):
        return None
    return number if math.isfinite(number) and number > 0 else None


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    index = (len(ordered) - 1) * fraction
    low, high = math.floor(index), math.ceil(index)
    return ordered[low] + (ordered[high] - ordered[low]) * (index - low)


def describe(values: list[float]) -> dict | None:
    if not values:
        return None
    return {
        "sample_count": len(values),
        "mean": round(statistics.fmean(values), 6),
        "p50": round(percentile(values, 0.50), 6),
        "p95": round(percentile(values, 0.95), 6),
        "maximum": round(max(values), 6),
    }


def capture_lines(stream):
    """Stop at the completed-capture marker without parsing its giant private footer."""
    while True:
        prefix = stream.readline(128)
        if not prefix:
            return
        if prefix.startswith("[HasHeaderRowAtEnd],1,") or prefix.rstrip("\r\n") == "[HasHeaderRowAtEnd],1":
            yield "[HasHeaderRowAtEnd],1\n"
            return
        yield prefix if prefix.endswith("\n") else prefix + stream.readline()


def read_capture(path: Path) -> dict:
    opener = gzip.open if path.suffix.lower() == ".gz" else open
    previous_limit = csv.field_size_limit()
    try:
        # Startup command output can occupy one large EVENTS field. Never export it.
        csv.field_size_limit(1024 * 1024)
        with opener(path, "rt", encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.reader(capture_lines(stream)))
    finally:
        csv.field_size_limit(previous_limit)
    headers = [
        index for index, row in enumerate(rows)
        if row and row[0].strip() == "EVENTS"
        and all(positive_number(value) is None for value in row[1:])
    ]
    if len(headers) < 2 or "FrameTime" not in rows[headers[-1]]:
        raise ValueError("No completed Unreal capture with its final header was found.")
    footer_index = headers[-1] + 1
    if (footer_index >= len(rows) or rows[footer_index][:2] != ["[HasHeaderRowAtEnd]", "1"]):
        raise ValueError("The completed-capture footer marker is missing.")
    # UE streaming CSV can append columns after the initial header.
    # Its final repeated header contains the complete, stable column order.
    header = rows[headers[-1]]
    for name in TIMINGS.values():
        if header.count(name) > 1:
            raise ValueError("A required timing column appears more than once.")
    indices = {
        name: header.index(name) for name in TIMINGS.values() if name in header
    }
    end = headers[-1] if len(headers) > 1 else len(rows)
    series: dict[str, list[float]] = {name: [] for name in TIMINGS.values()}
    count_names = ("ActorCount/BGUnit", "ActorCount/TotalActorCount")
    count_indices = {name: header.index(name) for name in count_names if name in header}
    counts: dict[str, list[int]] = {name: [] for name in count_names}
    invalid_positions = []
    frame_rows = 0
    for row in rows[headers[0] + 1:end]:
        if not row:
            continue
        if row[0].strip().startswith("["):
            break  # Never parse or export the [key],value metadata footer.
        row_position = frame_rows
        frame_rows += 1
        frame_index = indices["FrameTime"]
        frame = positive_number(row[frame_index]) if frame_index < len(row) else None
        if frame is None:
            invalid_positions.append(row_position)
            continue
        series["FrameTime"].append(frame)
        for name, index in indices.items():
            if name == "FrameTime":
                continue
            value = positive_number(row[index]) if index < len(row) else None
            if value is not None:
                series[name].append(value)
        for name, index in count_indices.items():
            value = positive_number(row[index]) if index < len(row) else None
            if value is not None and value.is_integer():
                counts[name].append(int(value))
    if not series["FrameTime"]:
        raise ValueError("The capture contains no positive finite frame timing samples.")
    return {
        "frame_count": len(series["FrameTime"]),
        "raw_frame_rows": frame_rows,
        "discarded_frame_rows": len(invalid_positions),
        "interior_invalid_frame_rows": sum(position not in (0, frame_rows - 1) for position in invalid_positions),
        "duration_seconds": round(sum(series["FrameTime"]) / 1000, 6),
        "timings": {key: describe(series[name]) for key, name in TIMINGS.items()},
        "unavailable_timings": [
            name for name in TIMINGS.values() if not series[name]
        ],
        "observed_actor_counts": {
            name: {"sample_count": len(values), "minimum": min(values), "maximum": max(values)}
            for name, values in counts.items() if values
        },
        "percentile_method": "Linear interpolation at (sample_count - 1) * percentile.",
        "units": "milliseconds",
    }


def selected_context(context: dict) -> dict:
    # Explicitly supplied context only. Raw engine metadata is never consulted.
    hardware = context.get("hardware", {})
    resolution = context.get("resolution", {})
    quality = context.get("quality", {})
    known = context.get("known_gameplay_actor_counts", {})
    selected_quality = {}
    for key in QUALITY_KEYS:
        record = quality.get(key, {})
        if not isinstance(record, dict):
            continue
        value, source = record.get("value"), record.get("source")
        if (isinstance(value, (int, float)) and math.isfinite(value)
                and source in ("runtime_query", "configured_only")):
            selected_quality[key] = {"value": value, "source": source}
    return {
        "engine_version": context.get("engine_version"),
        "build_configuration": context.get("build_configuration"),
        "scenario": context.get("scenario"),
        "hardware": {
            key: hardware[key] for key in
            ("cpu", "physical_cores", "logical_processors", "memory_gib", "gpu", "gpu_driver", "os", "os_version")
            if key in hardware
        },
        "resolution": {
            key: resolution[key] for key in ("width", "height", "mode", "source")
            if key in resolution
        },
        "quality": selected_quality,
        "known_gameplay_actor_counts": {
            key: known[key] for key in ("units", "vehicles", "doors")
            if isinstance(known.get(key), int) and known[key] >= 0
        },
        "total_actor_count": context.get("total_actor_count"),
        "warmup_seconds": context.get("warmup_seconds"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_file", type=Path)
    parser.add_argument("--context", type=Path, required=True,
                        help="Reviewed, safe hardware/resolution/scenario JSON; no raw engine metadata.")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--min-duration", type=float, default=30.0)
    args = parser.parse_args()
    if not math.isfinite(args.min_duration) or args.min_duration < 0:
        parser.error("--min-duration must be finite and nonnegative")
    try:
        measured = read_capture(args.csv_file)
        context = json.loads(args.context.read_text(encoding="utf-8-sig"))
        if measured["duration_seconds"] < args.min_duration:
            parser.error("The real captured frame duration is shorter than the required measurement.")
        summary = {
            "schema_version": 1,
            **selected_context(context),
            "capture": measured,
            "limitations": [
                "A bounded foundation scene, not a full mission or final-art stress test.",
                "Global thread timings include waits and do not identify individual CPU/GPU bottlenecks.",
                "Zero or absent GPU timing is unavailable, not proof of zero GPU cost.",
                "Total actors are unmeasured unless explicitly supplied; listed gameplay counts exclude scenery.",
            ],
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, TypeError, KeyError, csv.Error) as error:
        parser.error(f"Performance summary failed ({type(error).__name__}).")
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())