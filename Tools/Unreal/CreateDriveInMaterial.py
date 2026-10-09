"""Bake the HD drive-in material using MediaTexture's modern 2D output."""
import unreal

path = '/Game/Materials/M_SimCopterDriveIn'
lib = unreal.MaterialEditingLibrary
asset = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if asset is None:
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SimCopterDriveIn', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
# ConstructorHelpers roots the material/expression objects. Reuse nodes rather than
# deleting rooted expressions in a commandlet.
def node(cls, index=0, x=0, y=0):
    found = [e for e in lib.get_material_expressions(asset) if isinstance(e, cls)]
    return found[index] if len(found) > index else lib.create_material_expression(asset, cls, x, y)
asset.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
asset.set_editor_property('two_sided', True)
texture = node(unreal.MaterialExpressionTextureSampleParameter2D, x=-600)
texture.set_editor_property('parameter_name', 'VideoTexture')
# Runtime and default textures must use the same 2D output/sampler type.
media_path = '/Game/Materials/T_DriveInDefault'
media = unreal.EditorAssetLibrary.load_asset(media_path) if unreal.EditorAssetLibrary.does_asset_exist(media_path) else None
if media is None:
    media = unreal.AssetToolsHelpers.get_asset_tools().create_asset('T_DriveInDefault', '/Game/Materials', unreal.MediaTexture, unreal.MediaTextureFactoryNew())
media.set_editor_property('new_style_output', True)
unreal.EditorAssetLibrary.save_loaded_asset(media, False)
texture.set_editor_property('texture', media)
texture.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
colors = node(unreal.MaterialExpressionVertexColor, x=-600, y=250)
gain = node(unreal.MaterialExpressionScalarParameter, x=-300, y=350)
gain.set_editor_property('parameter_name', 'EmissiveNits')
gain.set_editor_property('default_value', 1.0)
mul = node(unreal.MaterialExpressionMultiply, 0, -300)
out = node(unreal.MaterialExpressionMultiply, 1, -100)
for a, pin, b, target in [(texture, '', mul, 'A'), (colors, '', mul, 'B'), (mul, '', out, 'A'), (gain, '', out, 'B')]:
    if not lib.connect_material_expressions(a, pin, b, target):
        raise RuntimeError('Material connection failed')
if not lib.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError('Emissive connection failed')
lib.recompile_material(asset)
unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
unreal.log('DRIVE_IN_MATERIAL_READY')
