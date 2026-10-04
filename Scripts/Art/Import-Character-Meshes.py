"""Import original BLACKGLASS character/van OBJ meshes with Unreal's legacy factory.
Run via UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
Owned meshes with unchanged source hashes are validated without reimport.
Changed owned sources require a new asset version; rooted meshes are never replaced.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
SOURCE = ROOT / "SourceArt" / "Characters"
REPORT = ROOT / "Saved" / "Verification" / "character-mesh-import.json"
OWNER_TAG = "BlackglassCharacterBuilder"
SOURCE_TAG = "BlackglassCharacterSourceSHA256"
OWNER_VERSION = "tailored_lofts_v1"
report = {"version": OWNER_VERSION, "validated": False, "assets": [], "errors": []}


def create_mesh(entry, source):
    options = unreal.FbxImportUI()
    for key, value in {"automated_import_should_detect_type": False, "import_as_skeletal": False,
                       "import_mesh": True, "import_materials": False, "import_textures": False,
                       "is_obj_import": True, "override_full_name": True,
                       "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH}.items():
        options.set_editor_property(key, value)
    data = options.get_editor_property("static_mesh_import_data")
    for key, value in {"combine_meshes": True, "auto_generate_collision": False,
                       "build_nanite": False, "generate_lightmap_u_vs": False,
                       "convert_scene": False, "convert_scene_unit": False,
                       "import_uniform_scale": 1.0,
                       "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():
        data.set_editor_property(key, value)
    task = unreal.AssetImportTask()
    for key, value in {"factory": unreal.FbxFactory(), "options": options,
                       "filename": str(source), "destination_path": "/Game/Characters",
                       "destination_name": source.stem, "automated": True,
                       "replace_existing": False, "save": False}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(entry["asset"])
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Legacy OBJ factory did not create expected StaticMesh: " + entry["asset"])
    material = unreal.load_asset("/Game/Materials/M_BGIndustrialSurface")
    if material is None:
        raise RuntimeError("Original BLACKGLASS surface material is missing")
    mesh.set_material(0, material)
    unreal.EditorAssetLibrary.set_metadata_tag(mesh, OWNER_TAG, OWNER_VERSION)
    unreal.EditorAssetLibrary.set_metadata_tag(mesh, SOURCE_TAG, entry["sha256"])
    if not unreal.EditorAssetLibrary.save_asset(entry["asset"], only_if_is_dirty=False):
        raise RuntimeError("Failed to save owned mesh: " + entry["asset"])
    return mesh


def import_mesh(entry):
    asset_path = entry["asset"]
    source = ROOT / entry["source"]
    if hashlib.sha256(source.read_bytes()).hexdigest() != entry["sha256"]:
        raise RuntimeError("Generated source manifest hash mismatch: " + source.name)
    existing = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    if existing is not None:
        if unreal.EditorAssetLibrary.get_metadata_tag(existing, OWNER_TAG) != OWNER_VERSION:
            raise RuntimeError("Refusing to replace an unowned mesh: " + asset_path)
        if unreal.EditorAssetLibrary.get_metadata_tag(existing, SOURCE_TAG) != entry["sha256"]:
            raise RuntimeError("Owned mesh source changed; generate a new asset version instead of overwriting: " + asset_path)
    mesh = existing if existing is not None else create_mesh(entry, source)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Owned asset is not a StaticMesh: " + asset_path)
    import_data = mesh.get_editor_property("asset_import_data")
    importer_class = import_data.get_class().get_name() if import_data else ""
    if importer_class != "FbxStaticMeshImportData":
        raise RuntimeError("Unexpected import metadata class: " + importer_class)
    asset_file = ROOT / "Content" / "Characters" / (source.stem + ".uasset")
    content = asset_file.read_bytes()
    prefixes = {str(Path.home()), str(Path.home()).replace("\\", "/"), "C:/Users/", "C:\\Users\\"}
    if any(prefix.encode(encoding) in content for prefix in prefixes for encoding in ("utf-8", "utf-16-le")):
        raise RuntimeError("Imported mesh retains private absolute source paths: " + source.stem)
    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    actual_extent = [extent.x, extent.y, extent.z]
    expected_extent = [(axis[1] - axis[0]) / 2 for axis in entry["bounds"]]
    expected_origin = [(axis[1] + axis[0]) / 2 for axis in entry["bounds"]]
    actual_origin = [bounds.origin.x, bounds.origin.y, bounds.origin.z]
    if any(abs(a - b) > .05 for a, b in zip(actual_extent, expected_extent)):
        raise RuntimeError("Imported bounds changed the authored scale or axes: " + source.stem)
    if any(abs(a - b) > .05 for a, b in zip(actual_origin, expected_origin)):
        raise RuntimeError("Imported bounds changed the authored pivot: " + source.stem)
    result = {"asset": asset_path, "source": entry["source"], "triangles_source": entry["triangles"],
        "importer": "FbxFactory", "import_data_class": importer_class,
        "skipped_unchanged_source": existing is not None, "bounds_extent": [extent.x, extent.y, extent.z],
        "private_paths_found": False, "sha256": hashlib.sha256(content).hexdigest()}
    report["assets"].append(result)
    unreal.log("BLACKGLASS_ORIGINAL_MESH " + json.dumps(result, sort_keys=True))


try:
    manifest = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
    if manifest["version"] != OWNER_VERSION or len(manifest["meshes"]) != 13:
        raise RuntimeError("Unexpected original mesh manifest")
    unreal.EditorAssetLibrary.make_directory("/Game/Characters")
    for entry in manifest["meshes"]:
        import_mesh(entry)
    report["validated"] = True
except Exception as error:
    report["errors"].append(str(error))
    raise
finally:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log("BLACKGLASS_CHARACTER_IMPORT_RESULT " + json.dumps(report, sort_keys=True))
