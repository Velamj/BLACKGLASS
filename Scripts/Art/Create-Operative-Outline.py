"""Original controlled-operative silhouette material; engine-supported custom depth."""
import json
from pathlib import Path
import unreal
ASSET = "/Game/Materials/M_BGOperativeOutline"
TAG = "BlackglassOutlineBuilder"
VERSION = "controlled_outline_v1"
report = {"asset": ASSET, "validated": False, "errors": []}
lib = unreal.MaterialEditingLibrary
def node(material, cls, x, y, **props):
    value = lib.create_material_expression(material, cls, x, y)
    if value is None:
        raise RuntimeError("Could not create outline material expression")
    for key, prop in props.items():
        value.set_editor_property(key, prop)
    return value
def connect(a, b, pin, output=""):
    if not lib.connect_material_expressions(a, output, b, pin):
        raise RuntimeError("Outline input connection failed: " + pin)
try:
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET):
        material = unreal.load_asset(ASSET)
        if unreal.EditorAssetLibrary.get_metadata_tag(material, TAG) != VERSION:
            raise RuntimeError("Existing outline material is not owned by this builder")
        cached = True
    else:
        cached = False
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_BGOperativeOutline", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    location = next(getattr(unreal.BlendableLocation, name) for name in dir(unreal.BlendableLocation)
                    if name.upper().endswith("SCENE_COLOR_AFTER_TONEMAPPING"))
    material.set_editor_property("blendable_location", location)
    if not cached:
        code = """
    float2 vp = GetViewportUV(Parameters);
    float owned = step(0.5, Stencil.r) * step(Stencil.r, 2.5);
    float hidden = owned * step(Depth.r + 3.0, CustomDepth.r);
    float nearOwned = 0.0, nearHidden = 0.0;
    float2 offsets[4] = {float2(1,0),float2(-1,0),float2(0,1),float2(0,-1)};
    for (int i=0; i<4; ++i) {
        float2 nvp = saturate(vp + offsets[i] * PixelSize * 1.5);
        float2 suv = ClampSceneTextureUV(ViewportUVToSceneTextureUV(nvp,25),25);
        float2 cuv = ClampSceneTextureUV(ViewportUVToSceneTextureUV(nvp,13),13);
        float2 duv = ClampSceneTextureUV(ViewportUVToSceneTextureUV(nvp,1),1);
        float stencil = SceneTextureLookup(suv,25,false).r;
        float nowned = step(0.5,stencil) * step(stencil,2.5);
        float cdepth = SceneTextureLookup(cuv,13,false).r;
        float depth = SceneTextureLookup(duv,1,false).r;
        nearOwned = max(nearOwned,nowned);
        nearHidden = max(nearHidden,nowned * step(depth+3.0,cdepth));
    }
    float edge = (1.0-owned) * max(nearHidden,nearOwned*.25);
    float selected = step(1.5,Stencil.r);
    float opacity = saturate(hidden * lerp(.26,.4,selected) + edge*.92);
    return lerp(SceneColor.rgb,float3(.22,.72,.63),opacity);
    """
        inputs = []
        for name in ("SceneColor", "Depth", "CustomDepth", "Stencil", "PixelSize"):
            item = unreal.CustomInput()
            item.set_editor_property("input_name", name)
            inputs.append(item)
        custom = node(material, unreal.MaterialExpressionCustom, 400, 0, code=code,
                      output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs=inputs)
        for index, (name, enum) in enumerate((
                ("SceneColor", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),
                ("Depth", unreal.SceneTextureId.PPI_SCENE_DEPTH),
                ("CustomDepth", unreal.SceneTextureId.PPI_CUSTOM_DEPTH),
                ("Stencil", unreal.SceneTextureId.PPI_CUSTOM_STENCIL))):
            sample = node(material, unreal.MaterialExpressionSceneTexture, 0, index*200,
                          scene_texture_id=enum)
            connect(sample, custom, name, "Color")
            if name == "SceneColor":
                connect(sample, custom, "PixelSize", "InvSize")
        if not lib.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError("Outline output connection failed")
    errors = list(lib.recompile_material(material))
    if errors:
        raise RuntimeError("Outline shader errors: " + "; ".join(errors))
    unreal.EditorAssetLibrary.set_metadata_tag(material, TAG, VERSION)
    if not unreal.EditorAssetLibrary.save_asset(ASSET, only_if_is_dirty=False):
        raise RuntimeError("Could not save outline material")
    report.update(validated=True, stage=str(location), stencil_values=[1,2], samples=16, cached_owned_import=cached)
    unreal.log("BLACKGLASS_OUTLINE_VERIFIED " + json.dumps(report))
except Exception as error:
    report["errors"].append(str(error))
    unreal.log_error("BLACKGLASS_OUTLINE_FAILED " + str(error))
    raise
finally:
    destination = Path(unreal.Paths.project_saved_dir()) / "Verification" / "outline-generation.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2), encoding="utf-8")
