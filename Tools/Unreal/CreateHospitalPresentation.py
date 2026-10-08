"""Hospital-only procedural cladding/helipad and synthesized metal-contact cue.

Keeps the original HO209 vertices, collision, roof height and beacon faces intact.
Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
"""
import unreal, math, random, struct, wave
from pathlib import Path

lib = unreal.MaterialEditingLibrary
folder = '/Game/Generated/CityAtlas'
repo = Path(unreal.Paths.project_dir()).parent

def material(name, code, roughness):
    path = folder + '/' + name
    mat = unreal.load_asset(path) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('used_with_nanite', True)
    def expr(cls): return lib.create_material_expression(mat, cls)
    position=expr(unreal.MaterialExpressionWorldPosition)
    local=expr(unreal.MaterialExpressionTransformPosition)
    local.set_editor_property('transform_source_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    assert lib.connect_material_expressions(position,'',local,'')
    uv=expr(unreal.MaterialExpressionTextureCoordinate)
    normal=expr(unreal.MaterialExpressionVertexNormalWS)
    custom=expr(unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('code',code)
    pins=[]
    for name in ['P','UV','N']:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    custom.set_editor_property('inputs',pins)
    for node,pin in [(local,'P'),(uv,'UV'),(normal,'N')]: assert lib.connect_material_expressions(node,'',custom,pin)
    rgb=expr(unreal.MaterialExpressionComponentMask)
    for channel in ['r','g','b']: rgb.set_editor_property(channel,True)
    assert lib.connect_material_expressions(custom,'',rgb,'')
    assert lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    alpha=expr(unreal.MaterialExpressionComponentMask);alpha.set_editor_property('a',True)
    assert lib.connect_material_expressions(custom,'',alpha,'')
    assert lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_ROUGHNESS)
    spec=expr(unreal.MaterialExpressionConstant);spec.set_editor_property('r',.3)
    assert lib.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    lib.layout_material_expressions(mat);lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)

FACADE = r'''
// Local centimetres, independent of the tiny repeating legacy atlas texels.
float u = abs(N.x) > abs(N.y) ? P.y : P.x;
float floorHeight = 165.0;
float2 cell = float2(frac((u+48.0)/96.0),frac(P.z/floorHeight));
float2 aa = max(fwidth(cell),float2(.004,.004));
float frame = smoothstep(.12-aa.x,.12+aa.x,cell.x)*(1-smoothstep(.88-aa.x,.88+aa.x,cell.x));
float glass = frame*smoothstep(.29-aa.y,.29+aa.y,cell.y)*(1-smoothstep(.76-aa.y,.76+aa.y,cell.y));
float seam = (1-smoothstep(.008,.023,min(cell.x,1-cell.x))) * .06;
float3 ivory = float3(.72,.76,.73) - seam;
float3 blue = lerp(float3(.025,.095,.12),float3(.13,.28,.32),cell.y);
float3 color = lerp(ivory,blue,glass);
float stripe = smoothstep(.06,.08,cell.y)*(1-smoothstep(.15,.17,cell.y));
color = lerp(color,float3(.025,.25,.27),stripe);
float mullion = 1-smoothstep(.012,.027,abs(cell.x-.5));
color = lerp(color,float3(.48,.58,.59),glass*mullion);
// Dark stone plinth and double glazed doors on the lower entrance wing.
color = lerp(color,float3(.09,.13,.14),1-smoothstep(22,26,P.z));
float door = (1-smoothstep(82,85,abs(P.x)))*(1-smoothstep(168,171,P.z))*step(P.y,-540);
float doorframe = max(1-smoothstep(2,4,abs(P.x)),smoothstep(76,82,abs(P.x)));
color = lerp(color,lerp(float3(.028,.14,.18),float3(.55,.63,.61),doorframe),door);
float grain = sin(P.x*1.3)*sin(P.y*1.7)*sin(P.z*1.1);
color += grain*.009*(1-glass)*(1-smoothstep(.5,2,length(fwidth(P))));
return float4(color,lerp(.78,.24,glass));
'''
HELIPAD = r'''
float2 p=UV-.5;
float aa=max(length(fwidth(UV)),.001);
float radius=length(p);
float ring=1-smoothstep(.017-aa,.017+aa,abs(radius-.405));
float bars=(1-smoothstep(.045-aa,.045+aa,abs(abs(p.x)-.115)))*(1-smoothstep(.235-aa,.235+aa,abs(p.y)));
float middle=(1-smoothstep(.16-aa,.16+aa,abs(p.x)))*(1-smoothstep(.04-aa,.04+aa,abs(p.y)));
float h=saturate(bars+middle);
float noise=sin(P.x*2.7)*sin(P.y*1.7)*.01*(1-smoothstep(.5,2,length(fwidth(P))));
float3 color=float3(.045,.15,.16)+noise;
color=lerp(color,float3(.86,.9,.84),max(ring,h));
return float4(color,.86);
'''
ROOF = r'''
float grain=sin(P.x*1.7)*sin(P.y*2.1)*.014*(1-smoothstep(.5,2,length(fwidth(P))));
float seam=1-smoothstep(.8,1.6,min(abs(frac(P.x/160)-.5),abs(frac(P.y/160)-.5))*160);
return float4(float3(.18,.215,.22)+grain-seam*.04,.9);
'''
material('M_HospitalFacade',FACADE,.75)
material('M_HospitalHelipad',HELIPAD,.85)
material('M_HospitalRoof',ROOF,.9)

# A short original metal crunch: low body thump, inharmonic ringing, granular scrape.
output=repo/'SimCopterRemake/Content/Audio/HelicopterVehicleImpact.wav'
output.parent.mkdir(parents=True,exist_ok=True)
rng=random.Random(2049); rate=48000; duration=.56; samples=[]; low=0
for i in range(int(rate*duration)):
    t=i/rate; noise=rng.uniform(-1,1); low=.75*low+.25*noise
    attack=min(1,t/.0015)
    body=.5*math.sin(2*math.pi*(95*t-38*t*t))*math.exp(-t*18)
    ring=sum(math.sin(2*math.pi*f*t)*math.exp(-t*d) for f,d in [(487,17),(913,22),(1573,28),(2317,34)])*.12
    scrape=(noise-low)*.38*math.exp(-t*12)*(0.65+.35*math.sin(t*630))
    samples.append((body+ring+scrape)*attack)
peak=max(abs(x) for x in samples)
with wave.open(str(output),'wb') as wav:
    wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(rate)
    wav.writeframes(b''.join(struct.pack('<h',int(x/peak*.86*32767)) for x in samples))
task=unreal.AssetImportTask();task.set_editor_property('filename',str(output));task.set_editor_property('destination_path',folder)
task.set_editor_property('destination_name','S_HelicopterVehicleImpact');task.set_editor_property('automated',True)
task.set_editor_property('replace_existing',True);task.set_editor_property('save',True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert unreal.load_asset(folder+'/S_HelicopterVehicleImpact')
atten = unreal.load_asset(folder+'/A_VehicleImpact') or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'A_VehicleImpact',folder,unreal.SoundAttenuation,unreal.SoundAttenuationFactory())
settings=atten.get_editor_property('attenuation')
settings.set_editor_property('attenuate',True)
settings.set_editor_property('spatialize',True)
settings.set_editor_property('attenuation_shape_extents',unreal.Vector(250,0,0))
settings.set_editor_property('falloff_distance',3000)
atten.set_editor_property('attenuation',settings)
assert unreal.EditorAssetLibrary.save_loaded_asset(atten)
unreal.log('HOSPITAL_ASSETS_READY: facade, preserved roof, analytic helipad H, metal collision cue')
