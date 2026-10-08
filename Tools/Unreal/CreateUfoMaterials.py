"""Build the user-requested saucer materials from the retained generated hull texture."""
from pathlib import Path
import unreal

DEST = '/Game/Art/UFO'
tools = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(DEST)
source = Path(unreal.Paths.project_content_dir()) / 'Art/UFO/UFO_Hull_BaseColor.png'
task = unreal.AssetImportTask()
task.filename = str(source.resolve())
task.destination_path = DEST
task.destination_name = 'T_UFO_Hull'
task.automated = True
task.replace_existing = True
task.save = True
tools.import_asset_tasks([task])
texture = unreal.load_asset(DEST + '/T_UFO_Hull')
assert texture
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
texture.set_editor_property('srgb', True)
texture.set_editor_property('power_of_two_mode', unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
texture.set_editor_property('max_texture_size', 1024)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
texture.set_editor_property('compression_no_alpha', True)
unreal.EditorAssetLibrary.save_loaded_asset(texture)

lib = unreal.MaterialEditingLibrary
def scalar(mat, value, prop):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionConstant)
    node.set_editor_property('r', value)
    lib.connect_material_property(node, '', prop)

def material(name, color, metallic, roughness, glow=False, textured=False):
    mat = unreal.load_asset(DEST + '/' + name) if unreal.EditorAssetLibrary.does_asset_exist(DEST + '/' + name) else None
    # Native constructor references root these assets during startup. Reuse existing graphs;
    # deleting their expressions in a commandlet asserts in UE 5.8's material editor.
    if mat: return
    mat = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    node = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant', unreal.LinearColor(*color))
    lib.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR if glow else unreal.MaterialProperty.MP_BASE_COLOR)
    if textured:
        sample = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample)
        sample.set_editor_property('texture', texture)
        lib.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    scalar(mat, metallic, unreal.MaterialProperty.MP_METALLIC)
    scalar(mat, roughness, unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)

material('M_UFO_Hull', (0.6, 0.65, 0.7, 1), 0.7, 0.32, textured=True)
material('M_UFO_Canopy', (0.015, 0.07, 0.10, 1), 0.35, 0.14)
material('M_UFO_Drive', (0.015, 2.2, 3.0, 1), 0.0, 0.5, glow=True)
assert texture.get_editor_property('max_texture_size') == 1024
assert texture.get_editor_property('power_of_two_mode') == unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO
for name in ('M_UFO_Hull', 'M_UFO_Canopy', 'M_UFO_Drive'):
    assert unreal.load_asset(DEST + '/' + name)
unreal.log('UFO_MATERIALS_READY')
