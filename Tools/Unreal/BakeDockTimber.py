"""Shared, distance-filtered timber for AP223 piers and the MA248 marina."""
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CitySurfaceDetail import custom, connect
from BakeCivicPresentation import NORMAL_CODE

CODE=r'''
struct Timber {
    float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
    float noise(float2 p) {
        float2 i=floor(p),f=frac(p); f=f*f*(3-2*f);
        return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y)-.5;
    }
};
Timber t;
float kind=dot(step(.5,Kind.rgb),float3(1,2,4));
float2 p=UV*16;
float pixel=max(max(fwidth(p.x),fwidth(p.y)),.01);
float fade=1-smoothstep(.15,1.2,pixel);
float age=t.noise(p*.12);
if (kind>3.5) { Relief=0; return float4(float3(.055,.060,.063)+age*.012+t.noise(p*5)*.008*fade,.95); }
// Long irregular grain and quiet weathering, with no specular sparkle at flight distance.
float bend=t.noise(p*.18)*1.2;
float grain=t.noise(float2(p.x*.23,p.y*4+bend))*fade;
float fiber=t.noise(float2(p.x*.8,p.y*14+bend))*fade;
float3 base=kind<.5?float3(.22,.125,.060):kind<1.5?float3(.16,.086,.037):
            kind<2.5?float3(.105,.065,.031):float3(.24,.145,.075);
float3 color=base*(1+age*.22+grain*.20+fiber*.08);
Relief=(grain*.05+fiber*.015)*fade;
return float4(color,.9);
'''

def main():
    lib=unreal.MaterialEditingLibrary
    root='/Game/Generated/CityAtlas'
    mat=unreal.load_asset(root+'/M_DockTimber')
    if not mat:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_DockTimber',root,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('tangent_space_normal',False)
    uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
    tint=lib.create_material_expression(mat,unreal.MaterialExpressionVertexColor)
    normal=lib.create_material_expression(mat,unreal.MaterialExpressionVertexNormalWS)
    position=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
    surface=custom(mat,'WeatheredDockTimber',CODE,['UV','Kind'],unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    relief=unreal.CustomOutput(); relief.set_editor_property('output_name','Relief')
    relief.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    surface.set_editor_property('additional_outputs',[relief])
    connect(uv,'',surface,'UV'); connect(tint,'',surface,'Kind')
    for channels,prop in [('rgb',unreal.MaterialProperty.MP_BASE_COLOR),('a',unreal.MaterialProperty.MP_ROUGHNESS)]:
        mask=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
        for c in 'rgba': mask.set_editor_property(c,c in channels)
        connect(surface,'',mask,''); lib.connect_material_property(mask,'',prop)
    spec=lib.create_material_expression(mat,unreal.MaterialExpressionConstant); spec.set_editor_property('r',.18)
    lib.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    bump=custom(mat,'TimberFineRelief',NORMAL_CODE,['Position','BaseNormal','Height'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for node,out,pin in [(position,'','Position'),(normal,'','BaseNormal'),(surface,'Relief','Height')]: connect(node,out,bump,pin)
    lib.connect_material_property(bump,'',unreal.MaterialProperty.MP_NORMAL)
    lib.layout_material_expressions(mat); lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False): raise RuntimeError('Timber material did not save')
    unreal.log('DOCK_TIMBER_BAKE_COMPLETE')

if __name__=='__main__': main()
