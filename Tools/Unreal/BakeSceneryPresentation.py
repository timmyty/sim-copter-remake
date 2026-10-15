"""Bake project-owned paving, park paths and the detailed moon material; no engine asset edits.

Run in UnrealEditor-Cmd with -run=pythonscript -script=<this file> -AllowCommandletRendering.
The existing Generated/CityAtlas cook rule includes all three materials.
"""
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CitySurfaceDetail import custom, connect, add_surface_detail

ROOT = '/Game/Generated/CityAtlas'
LIB = unreal.MaterialEditingLibrary

PAVING = r'''
float2 p = Position.xy / 12.0;
float row = floor(p.y);
p.x += fmod(abs(row), 2.0) * 0.5;
float2 cell = floor(p);
float2 edge = min(frac(p), 1.0-frac(p));
float aa = max(fwidth(p.x), fwidth(p.y));
float joint = 1.0-smoothstep(0.025,0.025+max(aa,0.012),min(edge.x,edge.y));
joint *= 1.0-smoothstep(0.35,1.5,aa);
float grain = frac(sin(dot(cell,float2(12.9898,78.233)))*43758.5453);
float3 stone = lerp(float3(0.20,0.205,0.20),float3(0.29,0.28,0.25),grain);
return lerp(stone,float3(0.11,0.12,0.12),joint*0.7);
'''

PATH = r'''
float2 p = Position.xy / 5.0;
float2 cell = floor(p);
float grain = frac(sin(dot(cell,float2(12.9898,78.233)))*43758.5453);
grain = lerp(grain,0.5,saturate(max(fwidth(p.x),fwidth(p.y))));
return lerp(float3(0.24,0.21,0.16),float3(0.34,0.30,0.22),grain);
'''

def constant(mat, value):
    n = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant)
    n.set_editor_property('r',value)
    return n

def material(name, code):
    path = ROOT+'/'+name
    mat = unreal.load_asset(path)
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('two_sided',True)
    pos = LIB.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
    color = custom(mat,name,code,['Position'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    connect(pos,'',color,'Position')
    normal = LIB.create_material_expression(mat,unreal.MaterialExpressionVertexNormalWS)
    add_surface_detail(mat,color,normal,constant(mat,1.0),constant(mat,0.0))
    LIB.connect_material_property(constant(mat,0.88),'',unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.connect_material_property(constant(mat,0.15),'',unreal.MaterialProperty.MP_SPECULAR)
    LIB.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
    return mat

def main():
    material('M_SceneryPaving',PAVING)
    material('M_ParkPath',PATH)
    path = ROOT+'/MI_SimCopterMoon'
    moon = unreal.load_asset(path)
    if not moon:
        moon = unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_SimCopterMoon',ROOT,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(moon,unreal.load_asset('/CelestialVault/Materials/M_Moon'))
    # Reuse the matching 1024px lunar albedo and relief maps. At the wider disc they still
    # oversample normal gameplay views; stronger relief and restrained earthshine retain detail.
    for name,value in {'Brightness':10.0,'Contrast':0.06,'NormalMapIntensity':0.6,
                       'EarthLightContribution':0.008,'LightDarkTransitionSmoothness':0.85}.items():
        LIB.set_material_instance_scalar_parameter_value(moon,name,value)
    for name,source in [('MoonAlbedo','T_Moon_EquirectangularHalf'),('Moon Normal','T_Moon_EquirectangularHalf_N')]:
        LIB.set_material_instance_texture_parameter_value(moon,name,unreal.load_asset('/CelestialVault/Textures/'+source))
    LIB.update_material_instance(moon)
    unreal.EditorAssetLibrary.save_loaded_asset(moon,only_if_is_dirty=False)
    unreal.log('SCENERY_PRESENTATION_BAKE_COMPLETE')

if __name__ == '__main__': main()
