# Rigs V01_DepoGorevlisiA onto a fitted copy of the UE5 SK_Mannequin skeleton.
# Run: blender -b --factory-startup V01_source.blend --python rig_v01.py
# Writes V01_rig.blend and SKM_V01_DepoGorevlisiA.fbx next to the source file.
import os
import bpy
from mathutils import Matrix, Vector

CHAR_DIR = os.path.dirname(bpy.data.filepath)
PROJECT = os.path.abspath(os.path.join(CHAR_DIR, "..", "..", ".."))
MANNY_FBX = os.path.join(PROJECT, "ArtSource", "Mannequin", "SKM_Manny_Simple.fbx")
MESH_NAME = "V01_DepoGorevlisiA"
TARGET_HEIGHT = 1.83  # DEADLINE_Karakter_Pipeline.md, V01
MEASURED_HEIGHT = 1.80  # joint targets below were measured at this height

# --- 1. Scale the Tripo mesh (normalized to ~1 m) to game height --------------
body = bpy.data.objects[MESH_NAME]
top = max((body.matrix_world @ v.co).z for v in body.data.vertices)
body.scale *= TARGET_HEIGHT / top
bpy.context.view_layer.objects.active = body
for o in bpy.context.selected_objects:
    o.select_set(False)
body.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# --- 2. Import Manny as reference ----------------------------------------------
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=MANNY_FBX)
imported = [o for o in bpy.data.objects if o not in before]
manny_arm = next(o for o in imported if o.type == 'ARMATURE')
manny_mesh = next(o for o in imported if o.type == 'MESH')
manny_arm.name = "MannyRef"
# FBX import parents Manny under a 0.01-scale empty; bake that into the data.
bpy.context.view_layer.update()
for o in bpy.context.selected_objects:
    o.select_set(False)
for o in (manny_arm, manny_mesh):
    mw = o.matrix_world.copy()
    o.parent = None
    o.matrix_world = mw
    o.select_set(True)
bpy.context.view_layer.objects.active = manny_arm
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.context.view_layer.update()
_mw = manny_arm.matrix_world
assert all(abs(_mw[i][j] - (i == j)) < 1e-4 for i in range(4) for j in range(4)), _mw

rig = manny_arm.copy()
rig.data = manny_arm.data.copy()
rig.name = "root"  # FBX exporter turns the armature object into the "root" bone
rig.data.name = "SK_V01"
bpy.context.scene.collection.objects.link(rig)

# --- 3. Joint targets (left side, armature space, metres) ----------------------
# Measured from mesh cross-sections of the T-posed V01 at MEASURED_HEIGHT.
L = {
    "pelvis": (0.0, 0.012, 0.960),
    "thigh_l": (0.100, 0.010, 0.935),
    "calf_l": (0.147, -0.005, 0.510),
    "foot_l": (0.175, 0.030, 0.090),
    "ball_l": (0.195, -0.100, 0.015),
    "clavicle_l": (0.020, 0.025, 1.460),
    "upperarm_l": (0.190, 0.030, 1.440),
    "lowerarm_l": (0.440, 0.040, 1.440),
    "hand_l": (0.685, 0.012, 1.443),
    "thumb_01_l": (0.700, -0.015, 1.435),
    "thumb_02_l": (0.745, -0.052, 1.432),
    "thumb_03_l": (0.775, -0.078, 1.432),
}
TIPS = {"thumb_03_l": (0.810, -0.097, 1.432), "ball_l": (0.195, -0.177, 0.015)}
# finger: (knuckle y, knuckle z, knuckle x, length, metacarpal y)
FINGERS = {
    "index": (-0.047, 1.445, 0.795, 0.105, -0.030),
    "middle": (-0.020, 1.445, 0.800, 0.107, -0.012),
    "ring": (0.010, 1.443, 0.795, 0.100, 0.008),
    "pinky": (0.032, 1.440, 0.785, 0.085, 0.020),
}
for f, (y, z, x, ln, my) in FINGERS.items():
    L[f"{f}_metacarpal_l"] = (0.705, my, 1.445)
    L[f"{f}_01_l"] = (x, y, z)
    L[f"{f}_02_l"] = (x + ln * 0.45, y, z)
    L[f"{f}_03_l"] = (x + ln * 0.75, y, z)
    TIPS[f"{f}_03_l"] = (x + ln, y, z)
# spine keeps Manny heights, shifted in Y to the V01 torso centre
SPINE_DY = {"spine_01": 0.035, "spine_02": 0.035, "spine_03": 0.035, "spine_04": 0.030,
            "spine_05": 0.020, "neck_01": 0.005, "neck_02": 0.0, "head": -0.005}

bones_old = {b.name: (b.matrix_local.copy(), b.length) for b in manny_arm.data.bones}
targets = {}
for name, p in L.items():
    targets[name] = Vector(p)
    if name.endswith("_l"):
        targets[name[:-2] + "_r"] = Vector((-p[0], p[1], p[2]))
tips = {}
for name, p in TIPS.items():
    tips[name] = Vector(p)
    tips[name[:-2] + "_r"] = Vector((-p[0], p[1], p[2]))
for name, dy in SPINE_DY.items():
    targets[name] = bones_old[name][0].translation + Vector((0, dy, 0))
# The mesh is scaled uniformly, so the measured joints scale with it.
for d in (targets, tips):
    for name in d:
        d[name] = d[name] * (TARGET_HEIGHT / MEASURED_HEIGHT)

# Chain child used to measure each bone's segment direction.
SEG_CHILD = {"spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "spine_04",
             "spine_04": "spine_05", "spine_05": "neck_01", "neck_01": "neck_02", "neck_02": "head"}
for s in ("l", "r"):
    SEG_CHILD.update({f"thigh_{s}": f"calf_{s}", f"calf_{s}": f"foot_{s}", f"foot_{s}": f"ball_{s}",
                      f"clavicle_{s}": f"upperarm_{s}", f"upperarm_{s}": f"lowerarm_{s}",
                      f"lowerarm_{s}": f"hand_{s}", f"thumb_01_{s}": f"thumb_02_{s}",
                      f"thumb_02_{s}": f"thumb_03_{s}"})
    for f in FINGERS:
        SEG_CHILD.update({f"{f}_metacarpal_{s}": f"{f}_01_{s}", f"{f}_01_{s}": f"{f}_02_{s}",
                          f"{f}_02_{s}": f"{f}_03_{s}"})


def rot_between(a, b):
    return a.normalized().rotation_difference(b.normalized()).to_matrix()


def frame(d, lat):
    d = d.normalized()
    lat = (lat - d * lat.dot(d)).normalized()
    return Matrix((d, lat, d.cross(lat))).transposed()


# --- 4. Compute new rest matrices ----------------------------------------------
new_mats = {}   # bone -> 4x4 armature-space rest matrix
new_len = {}
R_of = {}       # bone -> 3x3 rotation applied to its old orientation
order = [b.name for b in manny_arm.data.bones]  # parents come before children
parent_of = {b.name: (b.parent.name if b.parent else None) for b in manny_arm.data.bones}

def old_head(n):
    return bones_old[n][0].translation

for name in order:
    old_m, old_l = bones_old[name]
    par = parent_of[name]
    R_par = R_of.get(par, Matrix.Identity(3))
    if name.startswith("hand_"):
        s = name[-1]
        o_d = old_head(f"middle_01_{s}") - old_head(name)
        o_lat = old_head(f"index_01_{s}") - old_head(f"pinky_01_{s}")
        n_d = targets[f"middle_01_{s}"] - targets[name]
        n_lat = targets[f"index_01_{s}"] - targets[f"pinky_01_{s}"]
        R = frame(n_d, n_lat) @ frame(o_d, o_lat).inverted()
        head = targets[name]
    elif name in targets:
        head = targets[name]
        if name in SEG_CHILD:
            o_d = old_head(SEG_CHILD[name]) - old_head(name)
            n_d = targets[SEG_CHILD[name]] - head
        elif name in tips:
            gp = parent_of[name]
            o_d = old_head(name) - old_head(gp)  # old end bones inherit parent's direction
            n_d = tips[name] - head
        else:
            o_d = n_d = None
        R = R_par if o_d is None else rot_between(R_par @ o_d, n_d) @ R_par
    elif "twist" in name:
        # keep the fractional position along the parent segment
        seg_child = SEG_CHILD[par]
        a_o, b_o = old_head(par), old_head(seg_child)
        t = (old_head(name) - a_o).dot(b_o - a_o) / (b_o - a_o).length_squared
        head = targets[par].lerp(targets[seg_child], t)
        R = R_par
    elif name.startswith("ik_foot_") and name != "ik_foot_root":
        src = "foot_" + name[-1]
        head, R = targets[src], R_of[src]
    elif name == "ik_hand_gun":
        head, R = targets["hand_r"], R_of["hand_r"]
    elif name.startswith("ik_hand_") and name != "ik_hand_root":
        src = "hand_" + name[-1]
        head, R = targets[src], R_of[src]
    else:  # root-level helpers (interaction, center_of_mass, ik roots)
        assert par is None, f"unfitted bone {name}"
        head, R = old_m.translation.copy(), Matrix.Identity(3)
    R_of[name] = R
    m = (R @ old_m.to_3x3()).to_4x4()
    m.translation = head
    new_mats[name] = m
    new_len[name] = old_l

missing = [n for n in order if n not in new_mats]
assert not missing, missing

bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for eb in rig.data.edit_bones:
    eb.use_connect = False
for name in order:
    eb = rig.data.edit_bones[name]
    eb.matrix = new_mats[name]
    eb.length = new_len[name]
bpy.ops.object.mode_set(mode='OBJECT')

# --- 5. Weights: bend Manny into the new rest pose, then transfer --------------
bpy.context.view_layer.objects.active = manny_arm
bpy.ops.object.mode_set(mode='POSE')
for name in order:
    manny_arm.pose.bones[name].matrix = new_mats[name]
    bpy.context.view_layer.update()
bpy.ops.object.mode_set(mode='OBJECT')

fit = manny_mesh.copy()
fit.data = manny_mesh.data.copy()
fit.name = "MannyFit"
bpy.context.scene.collection.objects.link(fit)
fit.parent = None
fit.matrix_world = manny_mesh.matrix_world
bpy.context.view_layer.objects.active = fit
for mod in list(fit.modifiers):
    if mod.type == 'ARMATURE':
        mod.object = manny_arm
        bpy.ops.object.modifier_apply(modifier=mod.name)
    else:
        fit.modifiers.remove(mod)

body.vertex_groups.clear()
dt = body.modifiers.new("WeightTransfer", 'DATA_TRANSFER')
dt.object = fit
dt.use_vert_data = True
dt.data_types_verts = {'VGROUP_WEIGHTS'}
dt.vert_mapping = 'POLYINTERP_NEAREST'
dt.layers_vgroup_select_src = 'ALL'
dt.layers_vgroup_select_dst = 'NAME'
bpy.context.view_layer.objects.active = body
bpy.ops.object.datalayout_transfer(modifier=dt.name)
bpy.ops.object.modifier_apply(modifier=dt.name)

# Clean up: drop empty groups, smooth, cap to 4 influences, normalize.
bpy.ops.object.mode_set(mode='WEIGHT_PAINT')
bpy.ops.object.vertex_group_clean(group_select_mode='ALL', limit=0.01)
bpy.ops.object.vertex_group_smooth(group_select_mode='ALL', factor=0.5, repeat=2)
bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL', limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
bpy.ops.object.mode_set(mode='OBJECT')
used = set()
for v in body.data.vertices:
    for g in v.groups:
        if g.weight > 0:
            used.add(body.vertex_groups[g.group].name)
for vg in list(body.vertex_groups):
    if vg.name not in used:
        body.vertex_groups.remove(vg)
unweighted = sum(1 for v in body.data.vertices if not any(g.weight > 0 for g in v.groups))
print("RIG weighted groups:", len(body.vertex_groups), "unweighted verts:", unweighted)

# --- 6. Bind, drop reference objects, save ------------------------------------
body.parent = rig
body.matrix_parent_inverse = Matrix.Identity(4)
arm_mod = body.modifiers.new("Armature", 'ARMATURE')
arm_mod.object = rig
body.name = "SKM_V01_DepoGorevlisiA"

for o in imported + [fit]:
    bpy.data.objects.remove(o, do_unlink=True)
bpy.data.orphans_purge(do_recursive=True)

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(CHAR_DIR, "V01_rig.blend"), copy=False)

# --- 7. FBX for UE ------------------------------------------------------------
# Work in centimetres for export (not saved): with metres, the unit conversion lands
# as a x100 scale on the "root" bone and retargeted anims collapse onto the pelvis.
bpy.context.scene.unit_settings.scale_length = 0.01
body.parent = None
for o in (rig, body):
    o.scale = (100, 100, 100)
for o in bpy.context.scene.objects:
    o.select_set(o in (rig, body))
bpy.context.view_layer.objects.active = rig
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
body.parent = rig
body.matrix_parent_inverse = Matrix.Identity(4)
bpy.ops.export_scene.fbx(
    filepath=os.path.join(CHAR_DIR, "SKM_V01_DepoGorevlisiA.fbx"),
    use_selection=True, object_types={'ARMATURE', 'MESH'},
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
    add_leaf_bones=False, use_armature_deform_only=False,
    primary_bone_axis='Y', secondary_bone_axis='X',
    bake_anim=False, mesh_smooth_type='FACE',
    path_mode='COPY', embed_textures=True)
print("RIG done")
