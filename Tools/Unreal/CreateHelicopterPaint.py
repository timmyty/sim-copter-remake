"""Dedicated paint for all nine helicopters. Preserves GEO livery colors under scene lighting."""
import unreal

lib = unreal.MaterialEditingLibrary
path = '/Game/Materials/M_SimCopterHelicopterPaint'
mat = unreal.load_asset(path)
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_SimCopterHelicopterPaint', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property('two_sided', False)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)

def expr(cls):
    return lib.create_material_expression(mat, cls)

def scalar(name, value):
    node = expr(unreal.MaterialExpressionScalarParameter)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node

def wire(a, out, b, pin):
    assert lib.connect_material_expressions(a, out, b, pin), pin

def output(node, out, prop):
    assert lib.connect_material_property(node, out, prop), prop

color = expr(unreal.MaterialExpressionVertexColor)
output(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
output(scalar('Metallic', 0.04), '', unreal.MaterialProperty.MP_METALLIC)
output(scalar('Specular', 0.28), '', unreal.MaterialProperty.MP_SPECULAR)

# Local coordinates keep fine paint grain attached to the moving aircraft.
world = expr(unreal.MaterialExpressionWorldPosition)
local = expr(unreal.MaterialExpressionTransformPosition)
local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
wire(world, '', local, '')
grain = expr(unreal.MaterialExpressionCustom)
grain.set_editor_property('description', 'Aircraft paint grain; fades below a pixel')
grain.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
grain.set_editor_property('code', '''
float footprint = max(length(ddx(Position)), length(ddy(Position)));
float fade = 1.0 - smoothstep(0.25, 1.5, footprint);
float3 p = Position * 3.0;
float n = sin(p.x + sin(p.z)) * sin(p.y + cos(p.x)) * sin(p.z + cos(p.y));
return saturate(Roughness + n * 0.035 * fade);
''')
pins = []
for name in ['Position', 'Roughness']:
    pin = unreal.CustomInput()
    pin.set_editor_property('input_name', name)
    pins.append(pin)
grain.set_editor_property('inputs', pins)
wire(local, '', grain, 'Position')
wire(scalar('Roughness', 0.44), '', grain, 'Roughness')
output(grain, '', unreal.MaterialProperty.MP_ROUGHNESS)
lib.layout_material_expressions(mat)
lib.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('HELICOPTER_PAINT_READY: opaque dielectric paint, local grain, no city shading or emissive')
