"""Generate Koopa Troopa model with armature and animations.
Output: resources/models/koopa_troopa.glb
Run: blender --background --python tools/models/koopa_troopa.py

Blender Z-up. glTF exporter converts to Y-up automatically.
Koopa Troopa is ~1.0 units tall, centered at origin, facing +Y (becomes +Z in engine).
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

GREEN = (0.2, 0.65, 0.15)
YELLOW = (0.95, 0.85, 0.3)
WHITE = (0.95, 0.95, 0.9)
ORANGE = (0.9, 0.5, 0.1)

# ---- Bone Dimensions (Blender Z-up) ----

BONE_DEFS = {
    'Root':   {'head': (0, 0, 0),       'tail': (0, 0, 0.5),   'parent': None},
    'Body':   {'head': (0, 0, 0.3),     'tail': (0, 0, 0.7),   'parent': 'Root'},
    'Shell':  {'head': (0, -0.05, 0.5), 'tail': (0, -0.1, 0.8),'parent': 'Body'},
    'Head':   {'head': (0, 0.1, 0.7),   'tail': (0, 0.15, 0.95),'parent': 'Body'},
    'Arm.L':  {'head': (-0.3, 0, 0.55), 'tail': (-0.4, 0, 0.35),'parent': 'Body'},
    'Arm.R':  {'head': (0.3, 0, 0.55),  'tail': (0.4, 0, 0.35),'parent': 'Body'},
    'Leg.L':  {'head': (-0.15, 0, 0.3), 'tail': (-0.15, 0.05, 0),'parent': 'Root'},
    'Leg.R':  {'head': (0.15, 0, 0.3),  'tail': (0.15, 0.05, 0),'parent': 'Root'},
}

# Ordered so parents always come before children
BONE_ORDER = [
    'Root', 'Body', 'Shell', 'Head',
    'Arm.L', 'Arm.R', 'Leg.L', 'Leg.R',
]

# ---- Body Part Definitions ----
# Each: (primitive, location, scale, color, vertex_group)

BODY_PARTS = [
    # Shell (green, rounded on back)
    ('sphere', (0, -0.08, 0.6),   (0.28, 0.25, 0.22), GREEN, 'Shell'),
    # Shell bottom (flat green underside)
    ('cube', (0, -0.05, 0.45),    (0.24, 0.2, 0.05), GREEN, 'Shell'),
    # Body (yellow torso under/in front of shell)
    ('sphere', (0, 0.05, 0.5),    (0.18, 0.15, 0.18), YELLOW, 'Body'),
    # Belly (white front patch)
    ('sphere', (0, 0.12, 0.48),   (0.13, 0.06, 0.14), WHITE, 'Body'),
    # Head (yellow sphere)
    ('sphere', (0, 0.15, 0.82),   (0.14, 0.13, 0.14), YELLOW, 'Head'),
    # Beak/snout (orange)
    ('sphere', (0, 0.26, 0.78),   (0.06, 0.06, 0.04), ORANGE, 'Head'),
    # Left arm (yellow)
    ('cube', (-0.32, 0.02, 0.48), (0.06, 0.05, 0.14), YELLOW, 'Arm.L'),
    # Right arm (yellow)
    ('cube', (0.32, 0.02, 0.48),  (0.06, 0.05, 0.14), YELLOW, 'Arm.R'),
    # Left upper leg (yellow)
    ('cube', (-0.15, 0.03, 0.2),  (0.07, 0.06, 0.12), YELLOW, 'Leg.L'),
    # Right upper leg (yellow)
    ('cube', (0.15, 0.03, 0.2),   (0.07, 0.06, 0.12), YELLOW, 'Leg.R'),
    # Left shoe (white)
    ('cube', (-0.15, 0.08, 0.04), (0.08, 0.1, 0.04), WHITE, 'Leg.L'),
    # Right shoe (white)
    ('cube', (0.15, 0.08, 0.04),  (0.08, 0.1, 0.04), WHITE, 'Leg.R'),
]


def create_armature():
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    armature_obj = bpy.context.active_object
    armature_obj.name = 'KoopaArmature'
    armature = armature_obj.data
    armature.name = 'KoopaArmature'

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
    mesh_obj.name = 'KoopaTroopa'

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

    # 1. walk (20 frames, looping) — casual walk cycle
    action = create_action(armature_obj, 'walk')
    reset_pose(armature_obj, 1)
    # Frame 1: left leg forward, right leg back
    set_bone_rotation(armature_obj, 'Leg.L', (-25, 0, 0), 1)
    set_bone_rotation(armature_obj, 'Leg.R', (25, 0, 0), 1)
    set_bone_rotation(armature_obj, 'Arm.L', (15, 0, 0), 1)
    set_bone_rotation(armature_obj, 'Arm.R', (-15, 0, 0), 1)
    set_bone_rotation(armature_obj, 'Head', (3, 0, 0), 1)
    # Frame 5: passing
    reset_pose(armature_obj, 5)
    set_bone_rotation(armature_obj, 'Head', (-2, 0, 0), 5)
    # Frame 10: right leg forward, left leg back
    set_bone_rotation(armature_obj, 'Leg.R', (-25, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Leg.L', (25, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Arm.R', (15, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Arm.L', (-15, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Head', (3, 0, 0), 10)
    # Frame 15: passing
    reset_pose(armature_obj, 15)
    set_bone_rotation(armature_obj, 'Head', (-2, 0, 0), 15)
    # Frame 20: loop back to start
    set_bone_rotation(armature_obj, 'Leg.L', (-25, 0, 0), 20)
    set_bone_rotation(armature_obj, 'Leg.R', (25, 0, 0), 20)
    set_bone_rotation(armature_obj, 'Arm.L', (15, 0, 0), 20)
    set_bone_rotation(armature_obj, 'Arm.R', (-15, 0, 0), 20)
    set_bone_rotation(armature_obj, 'Head', (3, 0, 0), 20)
    push_to_nla(armature_obj, action)

    # 2. flee (12 frames, looping) — panicked run
    action = create_action(armature_obj, 'flee')
    reset_pose(armature_obj, 1)
    # Frame 1: left leg far forward, right leg far back, arms flailing
    set_bone_rotation(armature_obj, 'Leg.L', (-45, 0, 0), 1)
    set_bone_rotation(armature_obj, 'Leg.R', (40, 0, 0), 1)
    set_bone_rotation(armature_obj, 'Arm.L', (35, 0, -15), 1)
    set_bone_rotation(armature_obj, 'Arm.R', (-30, 0, 15), 1)
    # Head looks back over shoulder
    set_bone_rotation(armature_obj, 'Head', (5, 0, 30), 1)
    # Frame 4: passing, head center
    reset_pose(armature_obj, 4)
    set_bone_rotation(armature_obj, 'Head', (0, 0, 15), 4)
    # Frame 7: right leg far forward, left leg far back
    set_bone_rotation(armature_obj, 'Leg.R', (-45, 0, 0), 7)
    set_bone_rotation(armature_obj, 'Leg.L', (40, 0, 0), 7)
    set_bone_rotation(armature_obj, 'Arm.R', (35, 0, 15), 7)
    set_bone_rotation(armature_obj, 'Arm.L', (-30, 0, -15), 7)
    set_bone_rotation(armature_obj, 'Head', (5, 0, -25), 7)
    # Frame 10: passing
    reset_pose(armature_obj, 10)
    set_bone_rotation(armature_obj, 'Head', (0, 0, -10), 10)
    # Frame 12: loop back
    set_bone_rotation(armature_obj, 'Leg.L', (-45, 0, 0), 12)
    set_bone_rotation(armature_obj, 'Leg.R', (40, 0, 0), 12)
    set_bone_rotation(armature_obj, 'Arm.L', (35, 0, -15), 12)
    set_bone_rotation(armature_obj, 'Arm.R', (-30, 0, 15), 12)
    set_bone_rotation(armature_obj, 'Head', (5, 0, 30), 12)
    push_to_nla(armature_obj, action)

    # 3. defeated (10 frames, one-shot) — shell pops off, body shrinks
    action = create_action(armature_obj, 'defeated')
    reset_pose(armature_obj, 1)
    # Shell flies up and back
    set_bone_location(armature_obj, 'Shell', (0, -0.3, 0.5), 5)
    set_bone_rotation(armature_obj, 'Shell', (30, 0, 15), 5)
    # Arms drop to sides
    set_bone_rotation(armature_obj, 'Arm.L', (10, 0, 15), 5)
    set_bone_rotation(armature_obj, 'Arm.R', (10, 0, -15), 5)
    # Body shrinks slightly via root lowering
    set_bone_location(armature_obj, 'Root', (0, 0, -0.05), 5)
    # Hold final pose
    set_bone_location(armature_obj, 'Shell', (0, -0.4, 0.7), 10)
    set_bone_rotation(armature_obj, 'Shell', (45, 0, 20), 10)
    set_bone_rotation(armature_obj, 'Arm.L', (15, 0, 20), 10)
    set_bone_rotation(armature_obj, 'Arm.R', (15, 0, -20), 10)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.08), 10)
    set_bone_rotation(armature_obj, 'Head', (-10, 0, 0), 10)
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
    out_path = os.path.join(project_root, "resources", "models", "koopa_troopa.glb")

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
