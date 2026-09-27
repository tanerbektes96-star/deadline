# Bootstraps the shared NPC animation setup from V01 (DEADLINE_Karakter_Pipeline.md, section 6):
#   Shared/SK_Deadline_Human, IK_Manny, IK_Deadline_Human, RTG_Manny_to_Deadline_Human,
#   Shared/Anims/* (retargeted Unarmed set), BS_Locomotion, ABP_NPC_Base,
#   NPC/V01/SKM_V01_DepoGorevlisiA (+ materials) and BP_NPC_V01 (ADeadlineNPCCharacter) wired to ABP_NPC_Base.
# V01 is the first character, so this script owns Shared/ and rebuilds it from scratch.
# Later characters import onto the existing SK_Deadline_Human instead.
# Run: UnrealEditor-Cmd.exe DEADLINE_.uproject -ExecutePythonScript=<this file> -unattended -nosplash -nop4 -RenderOffscreen
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FBX = os.path.join(PROJECT, "ArtSource", "Characters", "V01_DepoGorevlisiA", "SKM_V01_DepoGorevlisiA.fbx")
SHARED = "/Game/Deadline/Characters/Shared"
ANIMS = SHARED + "/Anims"
NPC = "/Game/Deadline/Characters/NPC/V01"
OLD_LAYOUT = ["/Game/Characters/V01_DepoGorevlisiA", "/Game/Characters/Rigs"]  # first attempt
TEST_MAPS = "/Game/Deadline/Maps/Test"  # ue_test_npc_v01.py; references BP_NPC_V01
MANNY_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
SOURCE_ABP = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"
SOURCE_ANIMS_DIR = "/Game/Characters/Mannequins/Anims/Unarmed"
HEIGHT_CM = 183.0

SRC, TGT = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
X, W = unreal.AnimPoseExtensions, unreal.AnimPoseSpaces.WORLD
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("V01SETUP " + msg)


def create(name, path, cls, factory):
    return tools.create_asset(name, path, cls, factory)


def move(asset, new_path):
    old = asset.get_path_name().split(".")[0]
    assert eal.rename_asset(old, new_path), f"rename {old} -> {new_path} failed"
    return unreal.load_asset(new_path)


# --- 0. Clean slate ---------------------------------------------------------------
# Deleting and re-creating the same assets inside one editor session is unreliable
# (referenced assets survive, re-imports collide with pending-delete objects), so
# run_ue_setup_v01.ps1 clears these folders on disk before the editor starts.
for d in OLD_LAYOUT + [NPC, SHARED, TEST_MAPS]:
    assert not eal.list_assets(d, True, False), f"{d} is not empty: run via run_ue_setup_v01.ps1"

# --- 1. Import V01; its skeleton becomes the shared SK_Deadline_Human ------------------
# The legacy FBX path honours create_physics_asset; the Interchange path skipped it.
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
opts = unreal.FbxImportUI()
opts.import_mesh = True
opts.import_as_skeletal = True
opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
opts.skeleton = None
opts.create_physics_asset = True
opts.import_animations = False
opts.import_materials = True
opts.import_textures = True
opts.skeletal_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = NPC
task.automated = True
task.replace_existing = True
task.save = True
task.options = opts
tools.import_asset_tasks([task])
imported = [unreal.load_asset(p) for p in task.imported_object_paths]
log("imported: " + ", ".join(f"{a.get_name()}({a.get_class().get_name()})" for a in imported))
mesh = next(a for a in imported if isinstance(a, unreal.SkeletalMesh))
skeleton = move(mesh.skeleton, SHARED + "/SK_Deadline_Human")
# rename_asset only fixes the mesh/physics asset in memory: save them or they keep the dead path.
for p in eal.list_assets(NPC, True, False):
    eal.save_asset(p, only_if_is_dirty=False)
mesh = unreal.load_asset(NPC + "/SKM_V01_DepoGorevlisiA")
assert mesh.skeleton == skeleton, f"mesh still points at {mesh.skeleton}"
b = mesh.get_bounds()
log(f"mesh height={b.origin.z + b.box_extent.z:.1f}cm skeleton={skeleton.get_path_name()} "
    f"physics={mesh.get_editor_property('physics_asset')}")
ref = X.get_reference_pose(skeleton)
root_s = X.get_ref_bone_pose(ref, "root", unreal.AnimPoseSpaces.LOCAL).scale3d
assert abs(root_s.x - 1) < 1e-3 and X.get_ref_bone_pose(ref, "pelvis", W).translation.z > 50, "skeleton units"

manny = unreal.load_asset(MANNY_MESH)


# --- 2. IK Rigs -----------------------------------------------------------------------
def make_ik_rig(name, skm):
    rig = create(name, SHARED, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(skm)
    c.apply_auto_generated_retarget_definition()
    c.apply_auto_fbik()
    log(f"{name}: root={c.get_retarget_root()} chains={len(c.get_retarget_chains())}")
    return rig


ik_manny = make_ik_rig("IK_Manny", manny)
ik_human = make_ik_rig("IK_Deadline_Human", mesh)

# --- 3. Retargeter ------------------------------------------------------------------------
rtg = create("RTG_Manny_to_Deadline_Human", SHARED, unreal.IKRetargeter, unreal.IKRetargetFactory())
rc = unreal.IKRetargeterController.get_controller(rtg)
rc.set_ik_rig(SRC, ik_manny)
rc.set_ik_rig(TGT, ik_human)
rc.set_preview_mesh(SRC, manny)
rc.set_preview_mesh(TGT, mesh)
rc.add_default_ops()
rc.assign_ik_rig_to_all_ops(SRC, ik_manny)
rc.assign_ik_rig_to_all_ops(TGT, ik_human)
rc.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
unmapped = [str(ch.chain_name) for ch in unreal.IKRigController.get_controller(ik_human).get_retarget_chains()
            if str(rc.get_source_chain(ch.chain_name)) != str(ch.chain_name)]
log(f"chains unmapped: {unmapped}")
# Characters are bound in T-pose, Manny in A-pose: align the target retarget pose to the source.
pose = rc.create_retarget_pose("Human_APose", TGT)
rc.set_current_retarget_pose(pose, TGT)
rc.auto_align_all_bones(TGT, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
# No chain carries the ik_* bones, so they would stay at the T-pose. ABP_NPC_Base's foot IK
# (CR_Mannequin_FootIK) pulls the feet onto ik_foot_*, which pinned them to the ground in PIE:
# the legs squatted instead of stepping. Pin every ik_* bone to the bone it shadows, as in Manny.
pin = rc.add_retarget_op("/Script/IKRig.IKRetargetPinBoneOp")
pc = rc.get_op_controller(pin)
for src, dst in [("foot_l", "ik_foot_l"), ("foot_r", "ik_foot_r"), ("hand_r", "ik_hand_gun"),
                 ("hand_r", "ik_hand_r"), ("hand_l", "ik_hand_l")]:
    pc.set_bone_pair(src, dst)
rc.assign_ik_rig_to_all_ops(SRC, ik_manny)
rc.assign_ik_rig_to_all_ops(TGT, ik_human)
log("ops: " + ", ".join(str(rc.get_op_name(i)) for i in range(rc.get_num_retarget_ops()))
    + f" | pins={dict(pc.get_all_bone_pairs())}")
for a in (ik_manny, ik_human, rtg):
    eal.save_loaded_asset(a)

# --- 4. Batch retarget: the ABP pulls in its blendspace and jump anims; add the rest -------
sources = [eal.find_asset_data(SOURCE_ABP)]
for p in eal.list_assets(SOURCE_ANIMS_DIR, True, False):
    ad = eal.find_asset_data(p)
    if str(ad.asset_class_path.asset_name) == "AnimSequence":
        sources.append(ad)
inputs = unreal.IKRetargetBatchOperationInputs()
inputs.assets_to_retarget = sources
inputs.source_mesh = manny
inputs.target_mesh = mesh
inputs.ik_retarget_asset = rtg
inputs.target_path = ANIMS
inputs.include_referenced_assets = True
inputs.overwrite_existing_files = True
results = [unreal.load_asset(str(r.package_name)) for r in unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)]
log(f"retargeted {len(results)}: " + ", ".join(f"{a.get_name()}({a.get_class().get_name()})" for a in results))

# RunBatchRetarget duplicates the ABP but only returns animation assets.
abp = unreal.load_asset(ANIMS + "/" + SOURCE_ABP.rsplit("/", 1)[1])
assert isinstance(abp, unreal.AnimBlueprint), "retargeted ABP missing"
bs = next(a for a in results if isinstance(a, unreal.BlendSpace))
bs = move(bs, SHARED + "/BS_Locomotion")
abp = move(abp, SHARED + "/ABP_NPC_Base")
unreal.BlueprintEditorLibrary.compile_blueprint(abp)
eal.save_directory(SHARED)

# --- 5. Checks ----------------------------------------------------------------------------
reg = unreal.AssetRegistryHelpers.get_asset_registry()
dep_opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                 include_hard_package_references=True)
for a in (abp, bs):
    deps = [str(d) for d in reg.get_dependencies(a.get_path_name().split(".")[0], dep_opts) or []
            if str(d).startswith("/Game/")]
    log(f"{a.get_name()} skeleton={a.get_editor_property('target_skeleton' if a is abp else 'skeleton').get_name()} "
        f"game deps={deps}")

seqs = [a for a in results if isinstance(a, unreal.AnimSequence)]
bad = []
for anim in seqs:
    if anim.get_editor_property("skeleton") != skeleton:
        bad.append(anim.get_name() + ":skeleton")
        continue
    pose = X.get_anim_pose_at_time(anim, anim.get_play_length() * 0.4, unreal.AnimPoseEvaluationOptions())
    worst = 0.0
    for p, c in [("thigh_l", "calf_l"), ("calf_r", "foot_r"), ("upperarm_l", "lowerarm_l"),
                 ("lowerarm_r", "hand_r"), ("spine_01", "spine_05"), ("neck_01", "head")]:
        r = X.get_ref_bone_pose(pose, c, W).translation.distance(X.get_ref_bone_pose(pose, p, W).translation)
        n = X.get_bone_pose(pose, c, W).translation.distance(X.get_bone_pose(pose, p, W).translation)
        worst = max(worst, abs(n - r) / r)
    if worst > 0.02:
        bad.append(f"{anim.get_name()}:{worst:.1%}")
    ik_gap = max(X.get_bone_pose(pose, a, W).translation.distance(X.get_bone_pose(pose, b, W).translation)
                 for a, b in [("foot_l", "ik_foot_l"), ("foot_r", "ik_foot_r"), ("hand_l", "ik_hand_l")])
    if ik_gap > 1.0:
        bad.append(f"{anim.get_name()}:ik {ik_gap:.0f}cm")
# MM_Attack_02 and MM_WallJump stretch ~2.5% in the source Manny anims too; not a retarget fault.
log(f"sequences={len(seqs)} bone-length failures={bad}")
log(f"ABP_NPC_Base status={abp.get_editor_property('status')}")

# --- 6. BP_NPC_V01: a DeadlineNPCCharacter (AI + NavMesh patrol) using the mesh and ABP_NPC_Base ------------------------------
factory = unreal.BlueprintFactory()
factory.set_editor_property("parent_class", unreal.DeadlineNPCCharacter)
bp = create("BP_NPC_V01", NPC, None, factory)
cdo = unreal.get_default_object(bp.generated_class())
half = round(HEIGHT_CM / 2)
cdo.get_editor_property("capsule_component").set_editor_property("capsule_half_height", half)
m = cdo.get_editor_property("mesh")
m.set_editor_property("skeletal_mesh_asset", mesh)
m.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
m.set_editor_property("anim_class", abp.generated_class())
m.set_editor_property("relative_location", unreal.Vector(0, 0, -half))
m.set_editor_property("relative_rotation", unreal.Rotator(0, 0, 270))  # mesh faces -Y, actor faces +X
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_loaded_asset(bp)
for p in eal.list_assets(NPC, True, False) + eal.list_assets(SHARED, True, False):
    eal.save_asset(p, only_if_is_dirty=False)
log("DONE")
unreal.SystemLibrary.quit_editor()
