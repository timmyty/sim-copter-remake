"""Bake the single shared water-tower material; run as an Unreal Python commandlet.

City lettering is a small, mipmapped runtime mask shared by all towers in one city.
The existing /Game/Generated/CityAtlas cook rule includes this material.
"""
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CitySurfaceDetail import custom, connect

CODE = r'''
float2 aa = max(fwidth(UV), 0.0005);
float grainFade = 1.0-smoothstep(0.004,0.018,max(aa.x,aa.y));
float2 grainCell = floor(UV*float2(850,420));
float grain = (frac(sin(dot(grainCell,float2(12.9898,78.233)))*43758.5453)-0.5)*grainFade;
float3 blue = float3(0.035,0.105,0.155);
float3 ivory = float3(0.64,0.70,0.69);
if (Kind > 0.75) // Existing support columns and underside; no additional draw section.
{
    float foot = smoothstep(1.82,2.06,UV.y);
    return float4(lerp(blue*1.25,float3(0.22,0.25,0.24),foot)+grain*0.018,0.66);
}
if (Kind > 0.25) // Steel cap, with radial sheet joints.
{
    float2 p = UV-0.5;
    float a = atan2(p.y,p.x)*1.591549;
    float seam = 1-smoothstep(0.008,0.025+fwidth(a),min(frac(a),1-frac(a)));
    float rim = smoothstep(0.44,0.48,length(p));
    // Neutral galvanized steel across the entire lid, including its perimeter: no blue pool.
    return float4(lerp(float3(0.51,0.50,0.46),float3(0.35,0.34,0.31),rim)-seam*0.045+grain*0.016,0.72);
}
float v = UV.y;
float band = 1-smoothstep(0.055-aa.y,0.055+aa.y,v);
band = max(band,smoothstep(0.91-aa.y,0.91+aa.y,v));
float gold = 1-smoothstep(0.006,0.006+aa.y,abs(v-0.885));
float2 plate = UV*float2(10,4);
float2 edge = min(frac(plate),1-frac(plate));
float2 pa = max(fwidth(plate),0.001);
float seam = max(1-smoothstep(0.003,0.003+pa.x,edge.x),1-smoothstep(0.003,0.003+pa.y,edge.y));
seam *= 1-smoothstep(0.15,0.5,max(pa.x,pa.y));
float2 bolt = float2(edge.x-0.045,frac(v*32)-0.5);
float rivet = 1-smoothstep(0.014,0.025+max(pa.x,aa.y*32),length(bolt));
float streak = frac(sin(floor(UV.x*180)*73.19)*531.17);
float weather = (pow(saturate(v),8)+pow(saturate(1-v),16))*streak*0.07;
float3 color = lerp(ivory,blue,band);
color = lerp(color,float3(0.57,0.38,0.12),gold);
color -= seam*0.055 + rivet*0.035 + weather;
color += grain*0.014;
// Two equal labels wrap onto opposite tank sides, with the name fitted on the CPU once.
float2 labelUV = float2((frac(UV.x*2)-0.5)/0.66+0.5,(v-0.46)/0.34+0.5);
float inside = step(0,labelUV.x)*step(labelUV.x,1)*step(0,labelUV.y)*step(labelUV.y,1);
float lettering = Texture2DSample(Lettering,LetteringSampler,saturate(labelUV)).r*inside;
color = lerp(color,blue*0.66,lettering);
return float4(color,lerp(0.58,0.46,band)+weather);
'''


def preserve_mesh_normals(mat):
    # The legacy city pipeline supplies outward normals but also emits reversed triangles.
    # Cancel the two-sided shader's automatic normal flip, which otherwise makes roofs black.
    lib = unreal.MaterialEditingLibrary
    sign = lib.create_material_expression(mat, unreal.MaterialExpressionTwoSidedSign)
    normal = custom(mat, 'KeepAuthoredNormals', 'return float3(0,0,Sign);', ['Sign'], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    connect(sign, '', normal, 'Sign')
    lib.connect_material_property(normal, '', unreal.MaterialProperty.MP_NORMAL)


def main():
    lib = unreal.MaterialEditingLibrary
    root = '/Game/Generated/CityAtlas'
    mat = unreal.load_asset(root+'/M_WaterTower')
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_WaterTower', root, unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('two_sided', True)
    uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
    tint = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor)
    label = lib.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter)
    label.set_editor_property('parameter_name', 'CityLettering')
    label.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/Black'))
    surface = custom(mat, 'PaintedWaterTower', CODE, ['UV','Kind','Lettering'], unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    connect(uv, '', surface, 'UV')
    connect(tint, 'R', surface, 'Kind')
    connect(label, '', surface, 'Lettering')
    color = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask)
    for channel in ['r','g','b']:
        color.set_editor_property(channel, True)
    connect(surface, '', color, '')
    lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask)
    rough.set_editor_property('a', True)
    connect(surface, '', rough, '')
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    spec = lib.create_material_expression(mat, unreal.MaterialExpressionConstant)
    spec.set_editor_property('r', 0.25)
    lib.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    preserve_mesh_normals(mat)
    lib.layout_material_expressions(mat)
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError('Water tower material did not save')
    unreal.log('WATER_TOWER_PRESENTATION_BAKE_COMPLETE')


if __name__ == '__main__':
    main()
