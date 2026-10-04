"""Rebuild BLACKGLASS-owned visual assets in the installed Unreal editor."""
from pathlib import Path
import runpy
import unreal
root = Path(unreal.Paths.project_dir())
for relative in ("Scripts/Create-Foundation-Materials.py",
                 "Scripts/Art/Create-Operative-Outline.py",
                 "Scripts/Art/Import-Character-Meshes.py"):
    unreal.log("BLACKGLASS_VISUAL_BUILD " + relative)
    runpy.run_path(str(root / relative), run_name="__main__")
unreal.log("BLACKGLASS_VISUAL_ASSETS_COMPLETE")
