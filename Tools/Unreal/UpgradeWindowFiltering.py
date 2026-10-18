"""Rebuild the city window shader and enable cell-safe coverage mipmaps."""
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import CreateSimCopterMaterials as materials


def main():
    atlas = "/Game/Generated/CityAtlas"
    library = unreal.EditorAssetLibrary
    # Keep source mask pixels (including IDs) intact. Only sampling properties change.
    for path in library.list_assets(atlas, recursive=False):
        name = path.split("/")[-1].split(".")[0]
        if not name.startswith("T_CityWindowPage_"):
            continue
        mask = unreal.load_asset(path)
        mask.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
        mask.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        mask.set_editor_property("srgb", False)
        mask.set_editor_property("never_stream", True)
        library.save_loaded_asset(mask, only_if_is_dirty=False)
    parent_path = "/Game/Materials/M_SimCopterCitySurface"
    # UE 5.8 cannot clear rooted expression objects; rebuild only this generated parent.
    if library.does_asset_exist(parent_path):
        library.delete_asset(parent_path)
    materials.create_city_atlas_material()
    parent = unreal.load_asset(parent_path)
    if parent is None:
        raise RuntimeError("City window material failed to build")
    count = 0
    for path in library.list_assets(atlas, recursive=False):
        name = path.split("/")[-1].split(".")[0]
        if name.startswith("MI_CityPage_"):
            instance = unreal.load_asset(path)
            unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
            library.save_loaded_asset(instance, only_if_is_dirty=False)
            count += 1
    library.save_loaded_asset(parent, only_if_is_dirty=False)
    unreal.log(f"WINDOW FILTERING UPDATED: {count} city materials; stable occupancy and filtered coverage.")


if __name__ == "__main__":
    main()
