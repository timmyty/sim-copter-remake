import unreal
dest = '/Game/Materials'
name = 'M_SimCopterCabinGlass'
path = dest + '/' + name
mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, dest, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('two_sided', True)
    lib = unreal.MaterialEditingLibrary
    color = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant', unreal.LinearColor(0.12, 0.22, 0.28, 1))
    lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for value, prop in [(0.14, unreal.MaterialProperty.MP_OPACITY), (0.18, unreal.MaterialProperty.MP_ROUGHNESS), (0.25, unreal.MaterialProperty.MP_SPECULAR)]:
        node = lib.create_material_expression(mat, unreal.MaterialExpressionConstant)
        node.set_editor_property('r', value)
        lib.connect_material_property(node, '', prop)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
assert mat
unreal.log('HELICOPTER_GLASS_READY')
