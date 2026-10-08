"""Material graph helpers for land and building surfaces; no changes to original art or masks."""
import unreal

# Each mip down to level 5 still contains an independent colour for every 32px atlas cell.
# Clamp separately at BOTH sampled levels: clamping once before a trilinear sample leaks the
# adjacent cell at its coarser level. Derivatives come from unwrapped UVs, never from frac().
ATLAS_SAMPLE_CODE = r"""
float2 footprint = max(abs(ddx(LocalUV)), abs(ddy(LocalUV))) * 32.0;
float lod = clamp(log2(max(max(footprint.x, footprint.y), 1.0)), 0.0, 5.0);
float lo = floor(lod), hi = min(lo + 1.0, 5.0);
float2 uv = frac(LocalUV);
float insetLo = exp2(lo) / 64.0, insetHi = exp2(hi) / 64.0;
float2 a = (Cell + clamp(uv, insetLo, 1.0 - insetLo)) / 8.0;
float2 b = (Cell + clamp(uv, insetHi, 1.0 - insetHi)) / 8.0;
return lerp(Texture2DSampleLevel(Page, PageSampler, a, lo).rgb,
            Texture2DSampleLevel(Page, PageSampler, b, hi).rgb, frac(lod));
"""

GRAIN_CODE = r"""
if (LowPower > 0.5 || Weight <= 0.0) return float2(0.0, 0.0);
struct SurfaceNoise {
    float hash(float2 p) {
        float3 q = frac(float3(p.xyx) * 0.1031);
        q += dot(q, q.yzx + 33.33);
        return frac((q.x + q.y) * q.z);
    }
    float noise(float2 p) {
        float2 i = floor(p), f = frac(p);
        float2 u = f*f*(3.0-2.0*f);
        return lerp(lerp(hash(i), hash(i+float2(1,0)),u.x),
                    lerp(hash(i+float2(0,1)),hash(i+1),u.x),u.y) - 0.5;
    }
};
SurfaceNoise n;
float3 axis = abs(Normal);
float2 p = axis.z > 0.55 ? Position.xy : (axis.x > axis.y ? Position.yz : Position.xz);
float pixelSize = max(length(ddx(p)), length(ddy(p)));
float fineFade = 1.0 - smoothstep(Scale * 0.4, Scale * 1.6, pixelSize);
float midFade = 1.0 - smoothstep(Scale * 3.2, Scale * 12.8, pixelSize);
float fine = n.noise(p / Scale) * fineFade;
float middle = n.noise(p / (Scale * 8.0)) * midFade;
float macro = n.noise(p / (Scale * 64.0));
float enabled = saturate(Weight) * (1.0 - step(0.5, LowPower));
return float2((fine * 0.35 + middle * 0.45 + macro * 0.20) * enabled, fine * enabled);
"""

NORMAL_CODE = r"""
float3 n = normalize(BaseNormal);
float3 dx = ddx(Position), dy = ddy(Position);
float3 r1 = cross(dy,n), r2 = cross(n,dx);
float det = dot(dx,r1);
float height = Grain.y * Relief;
float3 gradient = (ddx(height)*r1 + ddy(height)*r2) / (abs(det) > 0.00001 ? det : 1.0);
return normalize(n - gradient);
"""


def custom(material, name, code, inputs, output_type):
    node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionCustom)
    node.set_editor_property("description", name)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    return node


def connect(source, output, target, pin):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f"City surface connection failed: {pin}")


def filtered_atlas(material, parameter, local_uv, cell):
    page = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter)
    page.set_editor_property("parameter_name", parameter)
    # A valid default also lets the parent compile before its instances are rebound.
    page.set_editor_property("texture", unreal.load_asset("/Game/Generated/CityAtlas/T_CitySurfacePage_2"))
    sample = custom(material, "CellSafeFilteredAtlas", ATLAS_SAMPLE_CODE, ["Page", "LocalUV", "Cell"], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    connect(page, "", sample, "Page")
    connect(local_uv, "", sample, "LocalUV")
    connect(cell, "", sample, "Cell")
    return sample


def add_surface_detail(material, color, normal, weight, low_power, terrain=False):
    pos = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionWorldPosition)
    grain = custom(material, "LandGrain" if terrain else "MasonryGrain", GRAIN_CODE,
                   ["Position", "Normal", "Weight", "LowPower", "Scale"], unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    scale = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
    scale.set_editor_property("parameter_name", "SurfaceGrainScale")
    scale.set_editor_property("default_value", 12.0 if terrain else 6.0)
    for source, out, pin in ((pos,"","Position"),(normal,"","Normal"),(weight,"","Weight"),(low_power,"","LowPower"),(scale,"","Scale")):
        connect(source,out,grain,pin)
    albedo = custom(material, "SurfaceColourVariation", "return Color * (1.0 + Grain.x * Strength);",
                    ["Color", "Grain", "Strength"], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    strength = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
    strength.set_editor_property("parameter_name", "SurfaceColorVariation")
    strength.set_editor_property("default_value", 0.45 if terrain else 0.18)
    connect(color,"",albedo,"Color")
    connect(grain,"",albedo,"Grain")
    connect(strength,"",albedo,"Strength")
    unreal.MaterialEditingLibrary.connect_material_property(albedo,"",unreal.MaterialProperty.MP_BASE_COLOR)
    relief = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
    relief.set_editor_property("parameter_name", "SurfaceReliefCm")
    relief.set_editor_property("default_value", 1.8 if terrain else 0.55)
    bump = custom(material, "SurfaceRelief", NORMAL_CODE, ["Position","BaseNormal","Grain","Relief"], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    for source, pin in ((pos,"Position"),(normal,"BaseNormal"),(grain,"Grain"),(relief,"Relief")):
        connect(source,"",bump,pin)
    unreal.MaterialEditingLibrary.connect_material_property(bump,"",unreal.MaterialProperty.MP_NORMAL)
    material.set_editor_property("tangent_space_normal",False)
    return grain
