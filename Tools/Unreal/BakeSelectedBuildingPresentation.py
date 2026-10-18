"""Bake shared filtered architectural surfaces for the explicitly selected SC2000 models."""
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CitySurfaceDetail import custom,connect

CODE=r'''
struct SurfaceDetail {
 float hash(float2 q){return frac(sin(dot(q,float2(127.1,311.7)))*43758.5453);}
 float noise(float2 p){float2 a=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(hash(a),hash(a+float2(1,0)),f.x),lerp(hash(a+float2(0,1)),hash(a+1),f.x),f.y)-.5;}
 float seamMask(float d,float w,float aa){return 1-smoothstep(max(0,w-aa*.5),w+aa*.5,d);}
};
SurfaceDetail d;
float kind=dot(step(.5,Kind),float4(1,2,4,8));
float2 p=UV*16,aa=max(fwidth(p),.015);
float fine=1-smoothstep(.3,1.5,max(aa.x,aa.y));
float grain=d.noise(p*4)*fine,stain=d.noise(p*.16);
float3 colors[16]={float3(.37,.39,.37),float3(.50,.48,.38),float3(.39,.085,.035),float3(.025,.052,.063),float3(.018,.024,.027),float3(.38,.021,.008),float3(.69,.45,.045),float3(.34,.39,.41),float3(.016,.066,.018),float3(.026,.105,.025),float3(.009,.065,.11),float3(.008,.065,.16),float3(.23,.13,.055),float3(.017,.022,.024),float3(.63,.65,.61),float3(.025,.30,.27)};
float3 color=colors[(int)clamp(kind,0,15)];float rough=.8;WindowMask=0;Relief=0;
float2 blocks=p/float2(7,3);blocks.x+=fmod(abs(floor(blocks.y)),2)*.5;
float2 e=min(frac(blocks),1-frac(blocks))*float2(7,3);
float mortar=max(d.seamMask(e.x,.05,aa.x),d.seamMask(e.y,.05,aa.y));
float variation=d.hash(floor(blocks))-.5;
if(kind<.5 || abs(kind-14)<.1){color+=variation*.025-mortar*.055+stain*.025;Relief=-mortar*.12;}
if(kind> .5 && kind<3.5){
 float glass=step(2.5,kind),brick=step(1.5,kind)*(1-glass);
 float2 pitch=lerp(float2(7,8),float2(5,5),glass);
 float2 q=frac(p/pitch),px=max(aa/pitch,.001);
 float2 width=lerp(float2(.27,.31),float2(.43,.40),glass);
 float pane=d.seamMask(abs(q.x-.5),width.x,px.x)*d.seamMask(abs(q.y-.52),width.y,px.y);
 float frame=d.seamMask(abs(q.x-.5),width.x+.04,px.x)*d.seamMask(abs(q.y-.52),width.y+.04,px.y);
 float mullion=d.seamMask(abs(q.x-.5),.018,px.x);
 color+=variation*.022-brick*mortar*.07;
 color=lerp(color,lerp(float3(.62,.61,.53),float3(.46,.50,.48),glass),frame);
 color=lerp(color,float3(.016,.043,.055)+q.y*.035+stain*.01,pane);
 color=lerp(color,float3(.38,.41,.38),pane*mullion);
 WindowMask=pane*(1-mullion);Relief=frame*.10-pane*.35;rough=lerp(.82,.24,WindowMask);
}
if(abs(kind-4)<.1){float2 roof=p/float2(8,22);float2 r=min(frac(roof),1-frac(roof))*float2(8,22);float seam=d.seamMask(r.x,.07,aa.x);color+=seam*.045-d.seamMask(r.y,.08,aa.y)*.02;Relief=seam*.12;rough=.68;}
if(abs(kind-7)<.1 || abs(kind-6)<.1 || abs(kind-15)<.1){float seam=d.seamMask(min(frac(p.x/12),1-frac(p.x/12))*12,.055,aa.x);color+=stain*.025-seam*.055;Relief=-seam*.1;rough=.4;}
if(abs(kind-8)<.1){float leaves=d.noise(p*1.9)*fine;color+=leaves*.09+stain*.06;Relief=leaves*.3;rough=.94;}
if(abs(kind-9)<.1){color+=d.noise(p*.7)*.04+grain*.04;Relief=grain*.12;rough=.95;}
if(abs(kind-10)<.1){float ripple=sin(p.x*.6+sin(p.y*.45))*sin(p.y*.6);color+=ripple*.018;Relief=ripple*.04;rough=.16;}
if(abs(kind-11)<.1){float2 cells=p/float2(3,5);float2 ce=min(frac(cells),1-frac(cells))*float2(3,5);float grid=max(d.seamMask(ce.x,.04,aa.x),d.seamMask(ce.y,.04,aa.y));color+=grid*.25+d.hash(floor(cells))*.025;Relief=grid*.06;rough=.19;}
if(abs(kind-12)<.1){float plank=floor(p.x/4);float seam=d.seamMask(min(frac(p.x/4),1-frac(p.x/4))*4,.06,aa.x);color+=sin(p.y*2+sin(p.x*4))*.018*fine-seam*.065+d.hash(float2(plank,0))*.04;Relief=-seam*.12;}
return float4(max(color+grain*.012,0),rough);
'''

def main():
    lib=unreal.MaterialEditingLibrary
    root='/Game/Generated/CityAtlas';name='M_SelectedBuildings'
    mat=unreal.load_asset(root+'/'+name) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('tangent_space_normal',False)
    uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
    tint=lib.create_material_expression(mat,unreal.MaterialExpressionVertexColor)
    normal=lib.create_material_expression(mat,unreal.MaterialExpressionVertexNormalWS)
    assert lib.connect_material_property(normal,'',unreal.MaterialProperty.MP_NORMAL)
    surface=custom(mat,'SelectedBuildingFinishes',CODE,['UV','Kind'],unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    mask=unreal.CustomOutput();mask.set_editor_property('output_name','WindowMask');mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    surface.set_editor_property('additional_outputs',[mask])
    connect(uv,'',surface,'UV')
    rgba=lib.create_material_expression(mat,unreal.MaterialExpressionAppendVector)
    connect(tint,'',rgba,'A');connect(tint,'A',rgba,'B');connect(rgba,'',surface,'Kind')
    relief=unreal.CustomOutput();relief.set_editor_property('output_name','Relief');relief.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    surface.set_editor_property('additional_outputs',[mask,relief])
    from BakeCivicPresentation import NORMAL_CODE
    position=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
    bump=custom(mat,'SelectedSurfaceRelief',NORMAL_CODE,['Position','BaseNormal','Height'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    connect(position,'',bump,'Position');connect(normal,'',bump,'BaseNormal');connect(surface,'Relief',bump,'Height')
    assert lib.connect_material_property(bump,'',unreal.MaterialProperty.MP_NORMAL)
    for channels,prop in [('rgb',unreal.MaterialProperty.MP_BASE_COLOR),('a',unreal.MaterialProperty.MP_ROUGHNESS)]:
        node=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
        for c in 'rgba':node.set_editor_property(c,c in channels)
        connect(surface,'',node,'');assert lib.connect_material_property(node,'',prop)
    glow=custom(mat,'SelectedBuildingWindows',r'''
#ifdef LUMEN_CARD_CAPTURE
return 0;
#endif
float kind=dot(step(.5,Kind),float4(1,2,4,8));
float2 pitch=(kind>2.5 && kind<3.5)?float2(5,5):float2(7,8);
float2 pane=floor(UV*16/pitch);
float id=floor(Origin.x*.01)+floor(Origin.y*.01)*71;
float roll=frac(sin(dot(pane+Seed+id,float2(12.9898,78.233)))*43758.5453);
float row=frac(sin(dot(float2(0,pane.y)+Seed+id,float2(39.3468,11.135)))*24634.6345);
float lit=max(step(roll,LitFraction),step(row,RowLitFraction));
return float3(1.25,.97,.56)*Windows*lit*saturate(Blend)*max(GlowNits,0);
''',['UV','Kind','Windows','Origin','Blend','Seed','LitFraction','RowLitFraction','GlowNits'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    connect(uv,'',glow,'UV');connect(rgba,'',glow,'Kind');connect(surface,'WindowMask',glow,'Windows')
    origin=lib.create_material_expression(mat,unreal.MaterialExpressionObjectPositionWS);connect(origin,'',glow,'Origin')
    collection=unreal.load_asset('/Game/Materials/MPC_SimCopterDayNight');assert collection
    for pin,param in [('Blend','NightBlend'),('Seed','WindowSeed'),('LitFraction','WindowLitFraction'),('RowLitFraction','WindowRowLitFraction'),('GlowNits','WindowGlowNits')]:
        node=lib.create_material_expression(mat,unreal.MaterialExpressionCollectionParameter)
        node.set_editor_property('collection',collection);node.set_editor_property('parameter_name',param);connect(node,'',glow,pin)
    assert lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.layout_material_expressions(mat);lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
    # A separate translucent section only for the Launch biosphere. Its landscape remains visible.
    glass=unreal.load_asset(root+'/M_SelectedBuildingGlass') or unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SelectedBuildingGlass',root,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(glass)
    glass.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    glass.set_editor_property('two_sided',True)
    glass.set_editor_property('used_with_instanced_static_meshes',True)
    glass.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    color=lib.create_material_expression(glass,unreal.MaterialExpressionConstant3Vector);color.set_editor_property('constant',unreal.LinearColor(.18,.35,.37,1))
    lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    fresnel=lib.create_material_expression(glass,unreal.MaterialExpressionFresnel)
    opacity=custom(glass,'BiosphereEdge','return .025+F*.19;',['F'],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    connect(fresnel,'',opacity,'F');lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    rough=lib.create_material_expression(glass,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.12);lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(glass);assert unreal.EditorAssetLibrary.save_loaded_asset(glass,only_if_is_dirty=False)
    unreal.log('SELECTED_BUILDINGS_BAKE_COMPLETE')

if __name__=='__main__':main()
