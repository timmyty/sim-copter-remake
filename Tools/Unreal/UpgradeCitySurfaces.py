"""Run with UnrealEditor-Cmd -ExecutePythonScript to bake the land/building surface upgrade.

Original textures, window-ID masks, sprites and water remain untouched. Filtered colour copies
have box-filtered mipmaps aligned to the 8x8 cell grid; CitySurfaceDetail caps sampling at mip 5.
"""
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import CreateSimCopterMaterials as materials

ATLAS = "/Game/Generated/CityAtlas"


def filtered_copy(source_path, destination_path):
    source = unreal.load_asset(source_path)
    if source is None:
        raise RuntimeError(f"Missing original city texture: {source_path}")
    # Export the actual asset's source pixels and replace-import the generated copy in place.
    # This refreshes future atlas edits while preserving references and avoids delete/recreate
    # races with Unreal's asset registry. Original assets and their import settings stay intact.
    output = Path(__file__).resolve().parents[2] / "Docs/scratchpad/city-surfaces/texture-sources"
    output.mkdir(parents=True, exist_ok=True)
    filename = output / (destination_path.rsplit("/", 1)[1] + ".png")
    export = unreal.AssetExportTask()
    export.object = source
    export.filename = str(filename)
    export.automated = True
    export.prompt = False
    export.replace_identical = True
    export.exporter = unreal.TextureExporterPNG()
    if not unreal.Exporter.run_asset_export_task(export):
        raise RuntimeError(f"Could not export source pixels: {source_path}")
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path, task.destination_name = destination_path.rsplit("/", 1)
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.factory = unreal.TextureFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(destination_path)
    if texture is None:
        raise RuntimeError(f"Could not create filtered city texture: {destination_path}")
    texture.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    # BC blocks straddle cells at the coarsest safe mip; uncompressed colour avoids that bleed.
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def main(rebuild_materials=True):
    pages = {}
    for path in unreal.EditorAssetLibrary.list_assets(ATLAS, recursive=False):
        name = path.split("/")[-1].split(".")[0]
        if name.startswith("T_CityPage_"):
            page_id = name[len("T_CityPage_"):]
            day = filtered_copy(path, f"{ATLAS}/T_CitySurfacePage_{page_id}")
            night_path = f"{ATLAS}/T_CityNightPage_{page_id}"
            night = filtered_copy(night_path, f"{ATLAS}/T_CitySurfaceNightPage_{page_id}") if unreal.EditorAssetLibrary.does_asset_exist(night_path) else day
            pages[page_id] = (day, night)
    low = filtered_copy(f"{ATLAS}/T_TerrainLow", f"{ATLAS}/T_LandSurfaceLow")
    if rebuild_materials:
        # These are new dedicated parents. Rebind EVERY referencing MIC below after rebuilding.
        for name in ("M_SimCopterCitySurface", "M_SimCopterLandSurface", "M_SimCopterCitySolidSurface"):
            path = f"/Game/Materials/{name}"
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                unreal.EditorAssetLibrary.delete_asset(path)
        materials.create_city_atlas_material()
        materials.create_terrain_material()
        materials.create_city_solid_surface_material()
    city = unreal.load_asset("/Game/Materials/M_SimCopterCitySurface")
    land = unreal.load_asset("/Game/Materials/M_SimCopterLandSurface")
    if city is None or land is None:
        raise RuntimeError("Enhanced city surface parents did not build")
    for page_id, (day, night) in pages.items():
        mic = unreal.load_asset(f"{ATLAS}/MI_CityPage_{page_id}")
        if mic is None:
            raise RuntimeError(f"Missing city page material {page_id}")
        unreal.MaterialEditingLibrary.set_material_instance_parent(mic, city)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mic, "SurfaceTexture", day)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mic, "SurfaceNightTexture", night)
        unreal.EditorAssetLibrary.save_loaded_asset(mic, only_if_is_dirty=False)
    for name, texture in (("MI_TerrainLow", low), ("MI_TerrainHigh", pages["13"][0])):
        mic = unreal.load_asset(f"{ATLAS}/{name}")
        unreal.MaterialEditingLibrary.set_material_instance_parent(mic, land)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mic, "SurfaceTexture", texture)
        unreal.EditorAssetLibrary.save_loaded_asset(mic, only_if_is_dirty=False)
    unreal.log(f"CITY SURFACES UPGRADED: {len(pages)} city pages, day/night colour, two land bands. Original art/masks/water preserved.")


if __name__ == "__main__":
    main()
