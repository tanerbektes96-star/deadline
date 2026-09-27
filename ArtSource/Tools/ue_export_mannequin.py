# Exports the UE5 Manny mesh (and its SK_Mannequin skeleton) to FBX for Blender rigging.
# Run: UnrealEditor-Cmd.exe DEADLINE_.uproject -run=pythonscript -script=<this file>
import os
import unreal

OUT_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                       "ArtSource", "Mannequin")
os.makedirs(OUT_DIR, exist_ok=True)

for asset_path, name in [
    ("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple", "SKM_Manny_Simple.fbx"),
    ("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle", "MM_Idle.fbx"),
]:
    asset = unreal.load_asset(asset_path)
    if asset is None:
        unreal.log_error("Missing asset: " + asset_path)
        continue
    opts = unreal.FbxExportOption()
    opts.ascii = False
    opts.collision = False
    opts.level_of_detail = False
    opts.export_morph_targets = False
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(OUT_DIR, name)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.options = opts
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log("EXPORT {} -> {} : {}".format(asset_path, task.filename, ok))

unreal.SystemLibrary.quit_editor()
