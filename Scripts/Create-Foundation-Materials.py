"""Build original BLACKGLASS surface art in the installed Unreal editor.
Run: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>
Seeded tileable TGA source art and an owned material graph; no downloaded artwork.
"""
import json
import math
import random
import struct
from pathlib import Path
import unreal

ASSET = "/Game/Materials/M_BlackglassSurface"
OWNER_TAG = "BlackglassMaterialBuilder"
OWNER_VERSION = "surface_v2_detail"
REPORT = Path(unreal.Paths.project_saved_dir()) / "Verification" / "material-generation.json"
SOURCE = Path(unreal.Paths.project_dir()) / "SourceArt" / "Materials"
report = {"asset": ASSET, "created": False, "validated": False, "errors": [], "textures": []}
LIB = unreal.MaterialEditingLibrary


def parameter_names(material):
    return {
        "vectors": [str(name) for name in LIB.get_vector_parameter_names(material)],
        "scalars": [str(name) for name in LIB.get_scalar_parameter_names(material)],
        "textures": [str(name) for name in LIB.get_texture_parameter_names(material)],
    }


def inspect_reference():
    reference = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    if reference is None:
        raise RuntimeError("Installed BasicShapeMaterial cannot be loaded")
    info = parameter_names(reference)
    report["installed_basic_shape"] = info
    unreal.log("BLACKGLASS_BASIC_SHAPE_PARAMETERS " + json.dumps(info, sort_keys=True))


def periodic_layer(size, cells, generator):
    grid = [[generator.random() for _ in range(cells)] for _ in range(cells)]
    output = []
    for y in range(size):
        py = y * cells / size
        iy, fy = int(py), py % 1.0
        sy = fy * fy * (3.0 - 2.0 * fy)
        for x in range(size):
            px = x * cells / size
            ix, fx = int(px), px % 1.0
            sx = fx * fx * (3.0 - 2.0 * fx)
            a, b = grid[iy % cells][ix % cells], grid[iy % cells][(ix + 1) % cells]
            c, d = grid[(iy + 1) % cells][ix % cells], grid[(iy + 1) % cells][(ix + 1) % cells]
            output.append((a + (b - a) * sx) * (1.0 - sy) + (c + (d - c) * sx) * sy)
    return output


def generate_texture(name, seed, asphalt):
    size = 512
    generator = random.Random(seed)
    broad = periodic_layer(size, 8, generator)
    middle = periodic_layer(size, 32, generator)
    fine = periodic_layer(size, 128, generator)
    pixels = bytearray(size * size * 3)
    for y in range(size):
        for x in range(size):
            index = y * size + x
            value = .5 + (broad[index] - .5) * .3 + (middle[index] - .5) * .25
            value += (fine[index] - .5) * .2 + (generator.random() - .5) * (.32 if asphalt else .15)
            if not asphalt:
                crack = (145 + 22 * math.sin(y * math.tau / size) + 8 * math.sin(y * math.tau * 3 / size)) % size
                if abs(x - crack) < 1.2 and (y // 43) % 5 != 0:
                    value -= .19
            byte = max(0, min(255, round(value * 255)))
            pixels[index * 3:index * 3 + 3] = bytes((byte, byte, byte))
    SOURCE.mkdir(parents=True, exist_ok=True)
    path = SOURCE / (name + ".tga")
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, size, size, 24, 0x20)
    path.write_bytes(header + pixels)
    return path


def portable_legacy_import_data(texture, source):
    # Interchange stores absolute source names inside its factory-node tree.
    # Retire only our owned tree; TextureFactory uses ordinary relative reimport data.
    existing = texture.get_editor_property("asset_import_data")
    if existing is not None and existing.get_class().get_name() != "AssetImportData":
        transient = unreal.find_object(None, "/Engine/Transient")
        if transient is None or not existing.rename(outer=transient):
            raise RuntimeError("Could not retire owned Interchange import data")
        # TextureFactory replaces the Texture2D and constructs plain import data.
        # AssetImportData itself is intentionally read-only in the Python API.


def validate_import_privacy(name, texture):
    data = (Path(unreal.Paths.project_content_dir()) / "Textures" / (name + ".uasset")).read_bytes()
    home = str(Path.home())
    prefixes = {home, home.replace("\\", "/"), "C:/Users/", "C:\\Users\\"}
    for prefix in prefixes:
        if any(prefix.encode(encoding) in data for encoding in ("utf-8", "utf-16-le")):
            raise RuntimeError("Saved texture retains a private import path")
    import_class = texture.get_editor_property("asset_import_data").get_class().get_name()
    if import_class != "AssetImportData":
        raise RuntimeError("Texture import retained Interchange metadata")
    report.setdefault("portable_imports", []).append({"asset": "/Game/Textures/" + name,
        "importer": "TextureFactory", "import_data_class": import_class, "private_paths_found": False})


def import_texture(name, seed, asphalt):
    path = generate_texture(name, seed, asphalt)
    asset_path = "/Game/Textures/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        texture = unreal.load_asset(asset_path)
        if unreal.EditorAssetLibrary.get_metadata_tag(texture, OWNER_TAG) not in ("texture_v1", OWNER_VERSION):
            raise RuntimeError("Texture exists without this builder's ownership tag")
        portable_legacy_import_data(texture, path)
    task = unreal.AssetImportTask()
    task.set_editor_property("factory", unreal.TextureFactory())
    task.set_editor_property("filename", str(path))
    task.set_editor_property("destination_path", "/Game/Textures")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(asset_path)
    if texture is None:
        raise RuntimeError("Texture import failed: " + asset_path)
    unreal.EditorAssetLibrary.set_metadata_tag(texture, OWNER_TAG, "texture_v1")
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError("Texture save failed")
    validate_import_privacy(name, texture)
    report["textures"].append({"asset": asset_path, "source": "SourceArt/Materials/" + path.name,
        "seed": seed, "size": [512, 512], "generator": "periodic value noise and original sparse wear"})
    return texture


def expression(material, expression_class, x, y, properties):
    node = LIB.create_material_expression(material, expression_class, x, y)
    if node is None:
        raise RuntimeError("Material expression creation failed")
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node


def connect(source, target, pin, output=""):
    if not LIB.connect_material_expressions(source, output, target, pin):
        raise RuntimeError("Material expression connection failed: " + pin)


def build_material(concrete):
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET):
        material = unreal.load_asset(ASSET)
        version = unreal.EditorAssetLibrary.get_metadata_tag(material, OWNER_TAG)
        if version not in ("surface_v1", OWNER_VERSION):
            raise RuntimeError("Material exists without this builder's ownership tag")
        if version == OWNER_VERSION:
            compile_errors = list(LIB.recompile_material(material))
            if compile_errors:
                raise RuntimeError("Existing owned material shader errors: " + "; ".join(compile_errors))
            return material
        LIB.delete_all_material_expressions(material)
    else:
        unreal.EditorAssetLibrary.make_directory("/Game/Materials")
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_BlackglassSurface", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew()
        )
        if material is None:
            raise RuntimeError("MaterialFactoryNew failed")
        report["created"] = True
    color = expression(material, unreal.MaterialExpressionVectorParameter, -600, -200,
        {"parameter_name": "Color", "default_value": unreal.LinearColor(.25, .28, .30, 1)})
    roughness = expression(material, unreal.MaterialExpressionScalarParameter, -200, 350,
        {"parameter_name": "Roughness", "default_value": .82})
    strength = expression(material, unreal.MaterialExpressionScalarParameter, -400, 200,
        {"parameter_name": "DetailStrength", "default_value": 1.0})
    scale = expression(material, unreal.MaterialExpressionScalarParameter, -1000, 550,
        {"parameter_name": "DetailScale", "default_value": 4.0})
    uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -1000, 400, {})
    scaled_uv = expression(material, unreal.MaterialExpressionMultiply, -800, 450, {})
    sample = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 400,
        {"parameter_name": "SurfaceDetail", "texture": concrete})
    remap = expression(material, unreal.MaterialExpressionMultiply, -400, 400, {"const_b": .22})
    bias = expression(material, unreal.MaterialExpressionAdd, -200, 200, {"const_b": .78})
    blend = expression(material, unreal.MaterialExpressionLinearInterpolate, 0, 100, {"const_a": 1.0})
    tinted = expression(material, unreal.MaterialExpressionMultiply, 200, -100, {})
    connect(uv, scaled_uv, "A")
    connect(scale, scaled_uv, "B")
    connect(scaled_uv, sample, "UVs")
    connect(sample, remap, "A", "RGB")
    connect(remap, bias, "A")
    connect(bias, blend, "B")
    connect(strength, blend, "Alpha")
    connect(color, tinted, "A")
    connect(blend, tinted, "B")
    if not LIB.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("Color graph connection failed")
    if not LIB.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS):
        raise RuntimeError("Roughness graph connection failed")
    compile_errors = list(LIB.recompile_material(material))
    if compile_errors:
        raise RuntimeError("Material shader errors: " + "; ".join(compile_errors))
    unreal.EditorAssetLibrary.set_metadata_tag(material, OWNER_TAG, OWNER_VERSION)
    if not unreal.EditorAssetLibrary.save_asset(ASSET, only_if_is_dirty=False):
        raise RuntimeError("Material asset save failed")
    return material


def validate(material):
    names = parameter_names(material)
    report["generated_parameters"] = names
    if "Color" not in names["vectors"] or "SurfaceDetail" not in names["textures"]:
        raise RuntimeError("Generated material is missing color/detail parameters")
    if not {"Roughness", "DetailScale", "DetailStrength"}.issubset(set(names["scalars"])):
        raise RuntimeError("Generated material is missing scalar parameters")
    report["material_detail_remap"] = [.78, 1.0]
    report["texture_samples"] = 1
    report["validated"] = True
    unreal.log("BLACKGLASS_ORIGINAL_MATERIAL_VERIFIED " + json.dumps(report, sort_keys=True))


try:
    inspect_reference()
    concrete = import_texture("T_BG_ConcreteDetail", 1337, False)
    import_texture("T_BG_AsphaltDetail", 8701, True)
    validate(build_material(concrete))
except Exception as error:
    report["errors"].append(str(error))
    unreal.log_error("BLACKGLASS_MATERIAL_BUILD_FAILED " + str(error))
    raise
finally:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
