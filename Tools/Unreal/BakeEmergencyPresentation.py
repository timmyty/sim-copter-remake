"""Bake facade details only; hospital helipad/roof and station atlas roofs are retained."""
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CitySurfaceDetail import custom,connect

CODE=r'''
float kind=dot(step(.5,Kind.rgb),float3(1,2,4));
float windows=step(128,UV.x);
float enginePaint=step(32,UV.x)*(1-step(96,UV.x));
float2 p=float2(UV.x-windows*256-enginePaint*64,UV.y)*16;
float2 aa=max(fwidth(p),.035);
float fade=1-smoothstep(.2,1.2,max(aa.x,aa.y));
float grain=sin(p.x*7.1)*sin(p.y*9.3)*.008*fade;
WindowMask=0;
float3 color; float rough=.8;
if(kind<1.5) {
    float police=step(.5,kind);
    float2 brick=p/float2(4,1.6); brick.x+=fmod(abs(floor(brick.y)),2)*.5;
    float2 edge=min(frac(brick),1-frac(brick))*float2(4,1.6);
    float joint=1-smoothstep(.04,.14,min(edge.x,edge.y));
    color=lerp(float3(.37,.075,.033)-joint*.045,float3(.035,.36,.49),police);
    float2 pitch=lerp(float2(13,29),float2(8.5,17),police);
    float2 q=frac((p-float2(0,48))/pitch);
    float2 pix=max(fwidth(q),.003);
    float wx=smoothstep(.18-pix.x,.18+pix.x,q.x)*(1-smoothstep(.82-pix.x,.82+pix.x,q.x));
    float wy=smoothstep(.18-pix.y,.18+pix.y,q.y)*(1-smoothstep(.81-pix.y,.81+pix.y,q.y));
    float opening=wx*wy*windows*lerp(step(48,p.y),1,police);
    float mullion=1-smoothstep(.025,.055,abs(q.x-.5));
    float sill=smoothstep(.81-pix.y,.81+pix.y,q.y)*(1-smoothstep(.86-pix.y,.86+pix.y,q.y))*wx*windows;
    color=lerp(color,float3(.38,.52,.52),sill);
    color=lerp(color,lerp(float3(.014,.055,.07),float3(.055,.18,.23),q.y),opening);
    color=lerp(color,lerp(float3(.11,.08,.055),float3(.35,.62,.65),police),opening*mullion);
    WindowMask=opening*(1-mullion); rough=lerp(.85,.27,WindowMask);
} else if(kind<2.5) { color=lerp(float3(.88,.20,.025),float3(.65,.025,.012),enginePaint); rough=.58; }
else if(kind<3.5) { color=float3(.55,.58,.55); }
else if(kind<4.5) { color=float3(.018,.024,.026); }
else if(kind<5.5) { color=float3(.32,.37,.39); rough=.38; }
else if(kind<6.5) { color=float3(.015,.095,.13); rough=.23; WindowMask=.6; }
else { color=float3(.80,.49,.035); rough=.4; }
return float4(max(color+grain,0),rough);
'''

def main():
    lib=unreal.MaterialEditingLibrary
    root='/Game/Generated/CityAtlas';name='M_EmergencyDetails'
    mat=unreal.load_asset(root+'/'+name) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('tangent_space_normal',False)
    uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
    tint=lib.create_material_expression(mat,unreal.MaterialExpressionVertexColor)
    normal=lib.create_material_expression(mat,unreal.MaterialExpressionVertexNormalWS)
    assert lib.connect_material_property(normal,'',unreal.MaterialProperty.MP_NORMAL)
    surface=custom(mat,'EmergencyFacadeFinishes',CODE,['UV','Kind'],unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    mask=unreal.CustomOutput();mask.set_editor_property('output_name','WindowMask');mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    surface.set_editor_property('additional_outputs',[mask])
    connect(uv,'',surface,'UV');connect(tint,'',surface,'Kind')
    for channels,prop in [('rgb',unreal.MaterialProperty.MP_BASE_COLOR),('a',unreal.MaterialProperty.MP_ROUGHNESS)]:
        node=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
        for c in 'rgba':node.set_editor_property(c,c in channels)
        connect(surface,'',node,'');assert lib.connect_material_property(node,'',prop)
    glow=custom(mat,'EmergencyNightWindows',r'''
#ifdef LUMEN_CARD_CAPTURE
return 0;
#endif
float2 pane=floor(UV*float2(1.4,1));
float id=floor(Origin.x*.01)+floor(Origin.y*.01)*71;
float roll=frac(sin(dot(pane+Seed+id,float2(12.9898,78.233)))*43758.5453);
float row=frac(sin(dot(float2(0,pane.y)+Seed+id,float2(39.3468,11.135)))*24634.6345);
float lit=max(step(roll,LitFraction),step(row,RowLitFraction));
return float3(1.25,.97,.56)*Windows*lit*saturate(Blend)*max(GlowNits,0);
''',['UV','Windows','Origin','Blend','Seed','LitFraction','RowLitFraction','GlowNits'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    connect(uv,'',glow,'UV');connect(surface,'WindowMask',glow,'Windows')
    origin=lib.create_material_expression(mat,unreal.MaterialExpressionObjectPositionWS);connect(origin,'',glow,'Origin')
    collection=unreal.load_asset('/Game/Materials/MPC_SimCopterDayNight');assert collection
    for pin,param in [('Blend','NightBlend'),('Seed','WindowSeed'),('LitFraction','WindowLitFraction'),('RowLitFraction','WindowRowLitFraction'),('GlowNits','WindowGlowNits')]:
        node=lib.create_material_expression(mat,unreal.MaterialExpressionCollectionParameter)
        node.set_editor_property('collection',collection);node.set_editor_property('parameter_name',param);connect(node,'',glow,pin)
    assert lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.layout_material_expressions(mat);lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
    unreal.log('EMERGENCY_PRESENTATION_BAKE_COMPLETE')

if __name__=='__main__':main()
