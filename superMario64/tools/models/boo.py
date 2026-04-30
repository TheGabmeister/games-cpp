"""Generate Boo model with armature and animations.
Output: resources/models/boo.glb
Run: blender --background --python tools/models/boo.py

Blender Z-up. glTF exporter converts to Y-up automatically.
Boo is ~1.2 units diameter, floating, centered at origin, facing +Y (becomes +Z in engine).
"""
import bpy
import bmesh
import os
import math
from mathutils import Vector, Euler, Quaternion, Matrix

# ---- Scene Setup ----

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    for action in bpy.data.actions:
        bpy.data.actions.remove(action)

def set_vertex_colors(obj, color):
    mesh = obj.data
    if not mesh.color_attributes:
        mesh.color_attributes.new(name="Color", type='FLOAT_COLOR', domain='CORNER')
    color_attr = mesh.color_attributes[0]
    for i in range(len(color_attr.data)):
        color_attr.data[i].color = (*color, 1.0)

# ---- Colors ----

WHITE = (0.92, 0.92, 0.95)
LIGHT_GRAY = (0.8, 0.8, 0.85)
DARK = (0.15, 0.1, 0.1)
PINK = (0.9, 0.4, 0.5)

# ---- Bone Dimensions (Blender Z-up) ----

BONE_DEFS = {
    'Root':  {'head': (0, 0, 0.6),    'tail': (0, 0, 1.0),   'parent': None},
    'Body':  {'head': (0, 0, 0.3),    'tail': (0, 0, 1.1),   'parent': 'Root'},
    'Arm.L': {'head': (-0.45, 0, 0.6),'tail': (-0.65, 0, 0.45),'parent': 'Body'},
    'Arm.R': {'head': (0.45, 0, 0.6), 'tail': (0.65, 0, 0.45),'parent': 'Body'},
}

# Ordered so parents always come before children
BONE_ORDER = [
    'Root', 'Body', 'Arm.L', 'Arm.R',
]

# ---- Body Part Definitions ----
# Each: (primitive, location, scale, color, vertex_group)

BODY_PARTS = [
    # Main body (large white sphere, slightly tapered — wider at top)
    ('sphere', (0, 0, 0.7),       (0.45, 0.42, 0.5), WHITE, 'Body'),
    # Lower body taper (slightly narrower bottom piece)
    ('sphere', (0, 0, 0.4),       (0.3, 0.28, 0.2), WHITE, 'Body'),
    # Tail/bottom point (small piece for teardrop feel)
    ('sphere', (0, 0, 0.22),      (0.15, 0.14, 0.1), WHITE, 'Body'),
    # Left arm (stubby white arm)
    ('sphere', (-0.5, 0.05, 0.6), (0.15, 0.1, 0.1), LIGHT_GRAY, 'Arm.L'),
    # Right arm (stubby white arm)
    ('sphere', (0.5, 0.05, 0.6),  (0.15, 0.1, 0.1), LIGHT_GRAY, 'Arm.R'),
    # Left eye (dark oval)
    ('sphere', (-0.13, 0.35, 0.8),(0.07, 0.04, 0.09), DARK, 'Body'),
    # Right eye (dark oval)
    ('sphere', (0.13, 0.35, 0.8), (0.07, 0.04, 0.09), DARK, 'Body'),
    # Mouth (dark oval opening)
    ('sphere', (0, 0.37, 0.6),    (0.12, 0.05, 0.08), DARK, 'Body'),
    # Tongue (small pink piece inside mouth)
    ('sphere', (0, 0.35, 0.56),   (0.06, 0.04, 0.04), PINK, 'Body'),
]


def create_armature():
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    armature_obj = bpy.context.active_object
    armature_obj.name = 'BooArmature'
    armature = armature_obj.data
    armature.name = 'BooArmature'

    # Remove default bone
    for b in armature.edit_bones:
        armature.edit_bones.remove(b)

    # Create bones
    for name in BONE_ORDER:
        d = BONE_DEFS[name]
        bone = armature.edit_bones.new(name)
        bone.head = Vector(d['head'])
        bone.tail = Vector(d['tail'])
        if d['parent']:
            bone.parent = armature.edit_bones[d['parent']]
        bone.use_connect = False

    bpy.ops.object.mode_set(mode='OBJECT')
    return armature_obj


def create_body_mesh():
    """Create all body parts as separate objects, then join."""
    objects = []

    for prim, loc, scl, color, vgroup in BODY_PARTS:
        if prim == 'cube':
            bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
        elif prim == 'sphere':
            bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=6, radius=1, location=loc)

        obj = bpy.context.active_object
        obj.scale = scl
        set_vertex_colors(obj, color)
        # Tag with custom property for vertex group assignment
        obj['vgroup'] = vgroup
        objects.append(obj)

    # Apply transforms
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    return objects


def assign_weights_and_join(objects, armature_obj):
    """Assign each body part fully to its designated vertex group, then join."""
    # First, create vertex groups on each object and assign all verts
    for obj in objects:
        vgroup_name = obj.get('vgroup', 'Root')
        vg = obj.vertex_groups.new(name=vgroup_name)
        vg.add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')

    # Join all into one mesh
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()

    mesh_obj = bpy.context.active_object
    mesh_obj.name = 'Boo'

    # Ensure all bone names exist as vertex groups (some may not have geometry)
    for name in BONE_ORDER:
        if name not in mesh_obj.vertex_groups:
            mesh_obj.vertex_groups.new(name=name)

    # Parent to armature with armature modifier
    mesh_obj.parent = armature_obj
    mod = mesh_obj.modifiers.new(name='Armature', type='ARMATURE')
    mod.object = armature_obj
    mod.use_vertex_groups = True

    return mesh_obj


# ---- Animation Helpers ----

def set_bone_rotation(armature_obj, bone_name, euler_xyz, frame):
    """Set a bone's rotation at a given frame (euler in degrees, Blender Z-up)."""
    bone = armature_obj.pose.bones.get(bone_name)
    if not bone:
        return
    bone.rotation_mode = 'XYZ'
    bone.rotation_euler = Euler((math.radians(euler_xyz[0]),
                                  math.radians(euler_xyz[1]),
                                  math.radians(euler_xyz[2])))
    bone.keyframe_insert(data_path="rotation_euler", frame=frame)


def set_bone_location(armature_obj, bone_name, offset, frame):
    """Set a bone's location offset at a given frame."""
    bone = armature_obj.pose.bones.get(bone_name)
    if not bone:
        return
    bone.location = Vector(offset)
    bone.keyframe_insert(data_path="location", frame=frame)


def reset_pose(armature_obj, frame):
    """Key all bones at rest pose for the given frame."""
    for bone in armature_obj.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = Euler((0, 0, 0))
        bone.location = Vector((0, 0, 0))
        bone.keyframe_insert(data_path="rotation_euler", frame=frame)
        bone.keyframe_insert(data_path="location", frame=frame)


def create_action(armature_obj, name):
    """Create a new action and assign it."""
    action = bpy.data.actions.new(name=name)
    armature_obj.animation_data.action = action
    return action


def push_to_nla(armature_obj, action):
    """Push current action to NLA track."""
    track = armature_obj.animation_data.nla_tracks.new()
    track.name = action.name
    strip = track.strips.new(action.name, int(action.frame_range[0]), action)
    armature_obj.animation_data.action = None


# ---- Animation Definitions ----

def create_animations(armature_obj):
    """Create gameplay animations as NLA tracks."""
    bpy.context.view_layer.objects.active = armature_obj
    if not armature_obj.animation_data:
        armature_obj.animation_data_create()

    bpy.ops.object.mode_set(mode='POSE')

    # 1. idle (24 frames, looping) — gentle floating bob, arms swaying
    action = create_action(armature_obj, 'idle')
    reset_pose(armature_obj, 1)
    # Float up slightly, arms sway forward
    set_bone_location(armature_obj, 'Root', (0, 0, 0.06), 7)
    set_bone_rotation(armature_obj, 'Arm.L', (10, 0, 5), 7)
    set_bone_rotation(armature_obj, 'Arm.R', (10, 0, -5), 7)
    # Peak of bob
    set_bone_location(armature_obj, 'Root', (0, 0, 0.1), 12)
    set_bone_rotation(armature_obj, 'Arm.L', (-5, 0, -3), 12)
    set_bone_rotation(armature_obj, 'Arm.R', (-5, 0, 3), 12)
    # Float back down, arms sway back
    set_bone_location(armature_obj, 'Root', (0, 0, 0.06), 18)
    set_bone_rotation(armature_obj, 'Arm.L', (8, 0, 4), 18)
    set_bone_rotation(armature_obj, 'Arm.R', (8, 0, -4), 18)
    # Loop back to start
    reset_pose(armature_obj, 24)
    push_to_nla(armature_obj, action)

    # 2. approach (16 frames, looping) — arms reaching forward, body lunges
    action = create_action(armature_obj, 'approach')
    reset_pose(armature_obj, 1)
    # Arms reach forward aggressively
    set_bone_rotation(armature_obj, 'Arm.L', (40, 0, 20), 1)
    set_bone_rotation(armature_obj, 'Arm.R', (40, 0, -20), 1)
    # Lunge forward
    set_bone_location(armature_obj, 'Root', (0, 0.08, 0.03), 4)
    set_bone_rotation(armature_obj, 'Arm.L', (55, 0, 25), 4)
    set_bone_rotation(armature_obj, 'Arm.R', (55, 0, -25), 4)
    set_bone_rotation(armature_obj, 'Body', (8, 0, 0), 4)
    # Pull back slightly
    set_bone_location(armature_obj, 'Root', (0, 0.03, -0.02), 9)
    set_bone_rotation(armature_obj, 'Arm.L', (35, 0, 15), 9)
    set_bone_rotation(armature_obj, 'Arm.R', (35, 0, -15), 9)
    set_bone_rotation(armature_obj, 'Body', (3, 0, 0), 9)
    # Lunge again
    set_bone_location(armature_obj, 'Root', (0, 0.1, 0.04), 13)
    set_bone_rotation(armature_obj, 'Arm.L', (60, 0, 28), 13)
    set_bone_rotation(armature_obj, 'Arm.R', (60, 0, -28), 13)
    set_bone_rotation(armature_obj, 'Body', (10, 0, 0), 13)
    # Loop back
    set_bone_rotation(armature_obj, 'Arm.L', (40, 0, 20), 16)
    set_bone_rotation(armature_obj, 'Arm.R', (40, 0, -20), 16)
    set_bone_location(armature_obj, 'Root', (0, 0, 0), 16)
    set_bone_rotation(armature_obj, 'Body', (0, 0, 0), 16)
    push_to_nla(armature_obj, action)

    # 3. shy (12 frames, one-shot) — arms cover face, shrinks back
    action = create_action(armature_obj, 'shy')
    reset_pose(armature_obj, 1)
    # Arms start folding inward to cover face
    set_bone_rotation(armature_obj, 'Arm.L', (50, 0, 60), 4)
    set_bone_rotation(armature_obj, 'Arm.R', (50, 0, -60), 4)
    set_bone_location(armature_obj, 'Root', (0, -0.08, -0.03), 4)
    set_bone_rotation(armature_obj, 'Body', (-5, 0, 0), 4)
    # Fully covering face, shrunk back
    set_bone_rotation(armature_obj, 'Arm.L', (70, 0, 75), 8)
    set_bone_rotation(armature_obj, 'Arm.R', (70, 0, -75), 8)
    set_bone_location(armature_obj, 'Root', (0, -0.15, -0.05), 8)
    set_bone_rotation(armature_obj, 'Body', (-8, 0, 0), 8)
    # Hold shy pose
    set_bone_rotation(armature_obj, 'Arm.L', (70, 0, 75), 12)
    set_bone_rotation(armature_obj, 'Arm.R', (70, 0, -75), 12)
    set_bone_location(armature_obj, 'Root', (0, -0.15, -0.05), 12)
    set_bone_rotation(armature_obj, 'Body', (-8, 0, 0), 12)
    push_to_nla(armature_obj, action)

    bpy.ops.object.mode_set(mode='OBJECT')


# ---- Main ----

def main():
    clear_scene()

    armature_obj = create_armature()
    body_parts = create_body_mesh()
    mesh_obj = assign_weights_and_join(body_parts, armature_obj)

    create_animations(armature_obj)

    # Select armature and mesh for export
    bpy.ops.object.select_all(action='DESELECT')
    armature_obj.select_set(True)
    mesh_obj.select_set(True)
    bpy.context.view_layer.objects.active = armature_obj

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, "..", ".."))
    out_path = os.path.join(project_root, "resources", "models", "boo.glb")

    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format='GLB',
        export_animations=True,
        export_skins=True,
        export_normals=True,
        export_nla_strips=True,
        use_selection=True,
    )
    print(f"Exported: {out_path}")


if __name__ == "__main__":
    main()
