"""Create BLACKGLASS's real foundation map in Unreal Editor.
Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
Runtime actors are owned by BGOperation; this map contains no duplicate actors.
"""
import unreal

MAP = "/Game/Maps/DepotBlock"
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError("Foundation map already exists; refusing to overwrite it")
game_mode = unreal.load_class(None, "/Script/Blackglass.BGOperation")
if game_mode is None:
    raise RuntimeError("Build BlackglassEditor before creating its map")
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if world is None:
    raise RuntimeError("Unreal failed to create the foundation world")
world.get_world_settings().set_editor_property("default_game_mode", game_mode)
if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
    raise RuntimeError("Unreal failed to save the foundation map")
unreal.log("BLACKGLASS_FOUNDATION_MAP_CREATED " + MAP)
