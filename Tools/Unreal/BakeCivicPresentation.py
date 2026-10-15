"""Bake City Hall's shared stone/metal material and the revised closed-tank finish."""
import sys
from pathlib import Path
import unreal
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CitySurfaceDetail import custom, connect
from BakeWaterTowerPresentation import main as bake_tower

CODE = r'''
struct CivicDetail {
    float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
    float noise(float2 p) {
        float2 i=floor(p), f=frac(p); f=f*f*(3-2*f);
        return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y)-.5;
    }
    float seamMask(float distance,float width,float pixel) {
        return 1-smoothstep(max(0,width-pixel*.5),width+pixel*.5,distance);
    }
};
CivicDetail d;
float kind=dot(step(.5,Kind.rgb),float3(1,2,4));
// Authored UVs are in original-model units / 16, so detail scale follows every instance.
float2 p=UV*16, aa=max(fwidth(p),.015);
float fineFade=1-smoothstep(.18,.9,max(aa.x,aa.y));
float grain=d.noise(p*3)*fineFade;
float age=d.noise(p*.24);
float2 blocks=p/float2(9,4.5);
blocks.x+=fmod(abs(floor(blocks.y)),2)*.5;
float2 e=min(frac(blocks),1-frac(blocks))*float2(9,4.5);
float joint=max(d.seamMask(e.x,.06,aa.x),d.seamMask(e.y,.06,aa.y));
float bevel=max(d.seamMask(e.x,.18,aa.x),d.seamMask(e.y,.18,aa.y))-joint;
float variation=d.hash(floor(blocks))-.5;
float vertical=1-smoothstep(.6,.9,abs(Normal.z));
Relief=0; WindowMask=0;
if (kind<.5) {
    // Ashlar limestone: narrow recessed mortar, worn arrises and quiet mineral variation.
    float3 color=float3(.39,.405,.385)+variation*.032+age*.025+grain*.014;
    color-=joint*.10; color+=bevel*.023;
    Relief=(-joint*.22+bevel*.07+grain*.08)*fineFade;
    return float4(color,.87+age*.035);
}
if (kind<1.5) {
    // Carved cornice courses and drip grooves on existing trim faces, not extra strips.
    float h=frac(p.y/2.0)*2;
    float groove=d.seamMask(min(h,2-h),.10,aa.y)*vertical;
    float lip=d.seamMask(abs(h-.24),.09,aa.y)*vertical;
    float bed=d.seamMask(min(frac(p.x/10),1-frac(p.x/10))*10,.045,aa.x);
    float3 color=float3(.46,.47,.44)+age*.025+grain*.012-groove*.09-bed*.055+lip*.035;
    Relief=(-groove*.18+lip*.12-bed*.12+grain*.05)*fineFade;
    return float4(color,.82);
}
if (kind<2.5) {
    if (UV.y>=32) {
        // Crown UV region: sixteen tapered stone ribs over dark inset roof panels.
        // These describe the reference's flared crown without separate rib meshes.
        float u=frac(UV.x),v=saturate(UV.y-32),pixel=max(fwidth(UV.x),.001);
        float rib=d.seamMask(min(u,1-u),lerp(.075,.15,v),pixel);
        float inset=(1-rib)*(1-smoothstep(.40,.72,v));
        float3 color=lerp(float3(.17,.18,.17),float3(.43,.44,.41),rib)-inset*.08;
        Relief=rib*.8+grain*.04;
        return float4(color+age*.018,.79);
    }
    // Gray lead-sheet roofing is consistent with the original monochrome roof/dome.
    float2 panel=p/float2(5.5,15);
    panel.y+=floor(panel.x)*.5;
    float2 pe=min(frac(panel),1-frac(panel))*float2(5.5,15);
    float fold=d.seamMask(pe.x,.075,aa.x);
    float lap=d.seamMask(pe.y,.075,aa.y);
    float edgeShadow=d.seamMask(abs(pe.x-.20),.095,aa.x);
    float sheet=d.hash(floor(panel))-.5;
    float3 metal=float3(.145,.165,.175)+sheet*.025+age*.018+grain*.008;
    metal+=fold*.045-lap*.035-edgeShadow*.028;
    float roofHeight=(fold*.45-lap*.12)*fineFade;
    float flatWing=step(.985,abs(Normal.z))*step(30,abs(p.x));
    // Paint the coping and narrow recessed gutter into the existing wing-roof polygons.
    float rimDistance=min(23-abs(abs(p.x)-55),57-abs(p.y-5));
    float coping=(1-smoothstep(2.0,2.25,rimDistance))*flatWing;
    float gutter=d.seamMask(abs(rimDistance-2.7),.42,max(aa.x,aa.y))*flatWing;
    float damp=(1-smoothstep(3,7,rimDistance))*flatWing*(1-coping);
    float copingJoint=max(d.seamMask(min(frac(p.x/10),1-frac(p.x/10))*10,.055,aa.x),
                          d.seamMask(min(frac(p.y/10),1-frac(p.y/10))*10,.055,aa.y));
    float3 stone=float3(.46,.47,.44)+age*.02-copingJoint*.075;
    float3 color=lerp(metal-damp*.025,stone,coping)-gutter*.055;
    Relief=lerp(roofHeight,-copingJoint*.15,coping)-gutter*.22;
    return float4(color,lerp(.70+age*.08,.84,coping));
}
if (kind<3.5) {
    // One UV unit per architectural bay/storey. Windows, pilasters, lintels and
    // sills are shaded relief on a single wall/drum surface, including the lantern.
    float2 q=frac(UV),px=max(fwidth(UV),.001);
    float wx=d.seamMask(abs(q.x-.5),.205,px.x);
    float wy=d.seamMask(abs(q.y-.51),.305,px.y);
    float window=wx*wy;
    float surround=d.seamMask(abs(q.x-.5),.24,px.x)*d.seamMask(abs(q.y-.51),.34,px.y);
    float pilaster=d.seamMask(min(q.x,1-q.x),.11,px.x);
    float sill=d.seamMask(abs(q.y-.17),.027,px.y)*d.seamMask(abs(q.x-.5),.28,px.x);
    float lintel=d.seamMask(abs(q.y-.85),.035,px.y)*d.seamMask(abs(q.x-.5),.28,px.x);
    float fluting=d.seamMask(abs(min(q.x,1-q.x)-.055),.014,px.x)*pilaster;
    float3 stone=float3(.39,.405,.385)+variation*.02+age*.018+grain*.012;
    stone+=pilaster*.085+sill*.055+lintel*.045-fluting*.035-(surround-window)*.045;
    float mullion=d.seamMask(abs(q.y-.49),.009,px.y);
    float3 glass=float3(.021,.032,.035)+mullion*.04;
    WindowMask=window*(1-mullion);
    Relief=pilaster*.65-fluting*.18+sill*.9+lintel*.45-window*.9;
    return float4(lerp(stone,glass,window),lerp(.85,.34,window));
}
if (kind<4.5) {
    float hedge=step(1024,p.x);
    float2 leafP=p-float2(hedge*2048,0);
    float clusters=d.noise(leafP*.6),leaves=d.noise(leafP*2.8)*fineFade;
    float3 lawn=float3(.045,.12,.025)+clusters*.025+leaves*.018;
    float3 foliage=float3(.025,.075,.018)+clusters*.045+leaves*.038;
    Relief=lerp(leaves*.14,leaves*.55+clusters*.3,hedge);
    return float4(lerp(lawn,foliage,hedge),.94);
}
if (kind<5.5) {
    Relief=(-joint*.16+grain*.06)*fineFade;
    return float4(float3(.32,.325,.30)+variation*.028-joint*.055+age*.025,.9);
}
if (kind<6.5) return float4(float3(.10,.075,.04)+grain*.015,.65);
// Rooftop housings: louvered sides and a static fan/grille on the existing top face.
float2 q=UV-.5,px=max(fwidth(UV),.001);
float top=step(.8,abs(Normal.z));
float fan=1-smoothstep(.31,.31+max(px.x,px.y),length(q));
float hub=1-smoothstep(.065,.065+max(px.x,px.y),length(q));
float grille=d.seamMask(min(frac(UV.x*18),1-frac(UV.x*18)),.14,px.x*18);
float vent=d.seamMask(abs(q.x),.39,px.x)*d.seamMask(abs(q.y),.33,px.y);
float slats=d.seamMask(min(frac(UV.y*9),1-frac(UV.y*9)),.22,px.y*9);
float3 casing=float3(.23,.245,.21)+age*.018;
float3 roof=lerp(casing,float3(.035,.045,.04)+grille*.055+hub*.12,fan);
float3 side=lerp(casing,float3(.07,.08,.065)+slats*.075,vent);
Relief=lerp(-vent*.2+slats*.12,-fan*.35+grille*.13,top);
return float4(lerp(side,roof,top),.79);
'''

NORMAL_CODE = r'''
float3 n=normalize(BaseNormal);
float3 dx=ddx(Position),dy=ddy(Position);
float3 r1=cross(dy,n),r2=cross(n,dx);
float det=dot(dx,r1);
float3 slope=(ddx(Height)*r1+ddy(Height)*r2)/(abs(det)>.00001?det:1);
// World-space normals bypass UE's automatic tangent-normal backface flip, preserving
// the legacy mesh's outward normals on both windings (MaterialTemplate.ush).
return normalize(n-clamp(slope,-.25,.25));
'''

def main():
    lib=unreal.MaterialEditingLibrary
    root='/Game/Generated/CityAtlas'
    mat=unreal.load_asset(root+'/M_CityHall')
    if not mat:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CityHall',root,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('two_sided',True)
    uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
    tint=lib.create_material_expression(mat,unreal.MaterialExpressionVertexColor)
    normal=lib.create_material_expression(mat,unreal.MaterialExpressionVertexNormalWS)
    surface=custom(mat,'CivicStoneAndMetal',CODE,['UV','Kind','Normal'],unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    relief=unreal.CustomOutput(); relief.set_editor_property('output_name','Relief')
    relief.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    window_mask=unreal.CustomOutput(); window_mask.set_editor_property('output_name','WindowMask')
    window_mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    surface.set_editor_property('additional_outputs',[relief,window_mask])
    connect(uv,'',surface,'UV'); connect(tint,'',surface,'Kind')
    connect(normal,'',surface,'Normal')
    for channels,prop in [('rgb',unreal.MaterialProperty.MP_BASE_COLOR),('a',unreal.MaterialProperty.MP_ROUGHNESS)]:
        mask=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
        for c in 'rgba': mask.set_editor_property(c,c in channels)
        connect(surface,'',mask,''); lib.connect_material_property(mask,'',prop)
    spec=lib.create_material_expression(mat,unreal.MaterialExpressionConstant); spec.set_editor_property('r',.3)
    lib.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    position=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition)
    bump=custom(mat,'CivicFineRelief',NORMAL_CODE,['Position','BaseNormal','Height'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for node,out,pin in [(position,'','Position'),(normal,'','BaseNormal'),(surface,'Relief','Height')]:
        connect(node,out,bump,pin)
    mat.set_editor_property('tangent_space_normal',False)
    if not lib.connect_material_property(bump,'',unreal.MaterialProperty.MP_NORMAL): raise RuntimeError('Relief connection failed')
    # Keep the new windows on the city's existing day/night lighting controls.
    glow=custom(mat,'CivicNightWindows',
        'return float3(1,.66,.32)*Windows*Blend*GlowNits;',
        ['Windows','Blend','GlowNits'],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    connect(surface,'WindowMask',glow,'Windows')
    collection=unreal.load_asset('/Game/Materials/MPC_SimCopterDayNight')
    if not collection: raise RuntimeError('Missing city day/night collection')
    for pin,param in [('Blend','NightBlend'),('GlowNits','WindowGlowNits')]:
        node=lib.create_material_expression(mat,unreal.MaterialExpressionCollectionParameter)
        node.set_editor_property('collection',collection); node.set_editor_property('parameter_name',param)
        connect(node,'',glow,pin)
    if not lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR): raise RuntimeError('Night window connection failed')
    lib.layout_material_expressions(mat); lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False): raise RuntimeError('City Hall material did not save')
    bake_tower()
    unreal.log('CIVIC_PRESENTATION_BAKE_COMPLETE')

if __name__=='__main__': main()
