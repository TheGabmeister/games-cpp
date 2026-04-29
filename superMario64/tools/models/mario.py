"""Generate Mario model with armature and 24 animations.
Output: resources/models/mario.glb
Run: blender --background --python tools/models/mario.py

Blender Z-up. glTF exporter converts to Y-up automatically.
Mario is ~1.5 units tall, centered at origin, facing +Y (becomes +Z in engine).
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

RED = (0.9, 0.15, 0.1)
BLUE = (0.15, 0.15, 0.8)
SKIN = (1.0, 0.8, 0.6)
BROWN = (0.45, 0.25, 0.1)

# ---- Bone Dimensions (Blender Z-up) ----
# Heights: feet=0, knees=0.25, hips=0.5, spine=0.65, chest=0.8, shoulders=0.9, head=1.05, head_top=1.25

BONE_DEFS = {
    'Root':        {'head': (0, 0, 0),      'tail': (0, 0, 0.5),   'parent': None},
    'Hips':        {'head': (0, 0, 0.5),    'tail': (0, 0, 0.65),  'parent': 'Root'},
    'Spine':       {'head': (0, 0, 0.65),   'tail': (0, 0, 0.8),   'parent': 'Hips'},
    'Chest':       {'head': (0, 0, 0.8),    'tail': (0, 0, 0.95),  'parent': 'Spine'},
    'Head':        {'head': (0, 0, 0.95),   'tail': (0, 0, 1.25),  'parent': 'Chest'},
    'UpperArm.L':  {'head': (-0.25, 0, 0.9), 'tail': (-0.35, 0, 0.7), 'parent': 'Chest'},
    'LowerArm.L':  {'head': (-0.35, 0, 0.7), 'tail': (-0.35, 0, 0.48), 'parent': 'UpperArm.L'},
    'Hand.L':      {'head': (-0.35, 0, 0.48),'tail': (-0.35, 0, 0.4), 'parent': 'LowerArm.L'},
    'UpperArm.R':  {'head': (0.25, 0, 0.9),  'tail': (0.35, 0, 0.7), 'parent': 'Chest'},
    'LowerArm.R':  {'head': (0.35, 0, 0.7),  'tail': (0.35, 0, 0.48), 'parent': 'UpperArm.R'},
    'Hand.R':      {'head': (0.35, 0, 0.48), 'tail': (0.35, 0, 0.4), 'parent': 'UpperArm.R'},
    'UpperLeg.L':  {'head': (-0.15, 0, 0.5), 'tail': (-0.2, 0, 0.25), 'parent': 'Hips'},
    'LowerLeg.L':  {'head': (-0.2, 0, 0.25), 'tail': (-0.2, 0, 0.05), 'parent': 'UpperLeg.L'},
    'Foot.L':      {'head': (-0.2, 0, 0.05), 'tail': (-0.2, 0.15, 0.02), 'parent': 'LowerLeg.L'},
    'UpperLeg.R':  {'head': (0.15, 0, 0.5),  'tail': (0.2, 0, 0.25), 'parent': 'Hips'},
    'LowerLeg.R':  {'head': (0.2, 0, 0.25),  'tail': (0.2, 0, 0.05), 'parent': 'UpperLeg.R'},
    'Foot.R':      {'head': (0.2, 0, 0.05),  'tail': (0.2, 0.15, 0.02), 'parent': 'LowerLeg.R'},
}

# Ordered so parents always come before children
BONE_ORDER = [
    'Root', 'Hips', 'Spine', 'Chest', 'Head',
    'UpperArm.L', 'LowerArm.L', 'Hand.L',
    'UpperArm.R', 'LowerArm.R', 'Hand.R',
    'UpperLeg.L', 'LowerLeg.L', 'Foot.L',
    'UpperLeg.R', 'LowerLeg.R', 'Foot.R',
]

# ---- Body Part Definitions ----
# Each: (primitive, location, scale, color, vertex_group)

BODY_PARTS = [
    # Legs (blue)
    ('cube', (-0.15, 0, 0.25), (0.1, 0.1, 0.25), BLUE, 'UpperLeg.L'),
    ('cube', (0.15, 0, 0.25),  (0.1, 0.1, 0.25), BLUE, 'UpperLeg.R'),
    # Lower legs
    ('cube', (-0.2, 0, 0.15),  (0.08, 0.08, 0.15), BLUE, 'LowerLeg.L'),
    ('cube', (0.2, 0, 0.15),   (0.08, 0.08, 0.15), BLUE, 'LowerLeg.R'),
    # Torso (red)
    ('cube', (0, 0, 0.7),      (0.25, 0.18, 0.2), RED, 'Spine'),
    # Chest
    ('cube', (0, 0, 0.88),     (0.23, 0.16, 0.1), RED, 'Chest'),
    # Arms (red)
    ('cube', (-0.35, 0, 0.75), (0.08, 0.08, 0.15), RED, 'UpperArm.L'),
    ('cube', (0.35, 0, 0.75),  (0.08, 0.08, 0.15), RED, 'UpperArm.R'),
    # Lower arms
    ('cube', (-0.35, 0, 0.55), (0.06, 0.06, 0.12), RED, 'LowerArm.L'),
    ('cube', (0.35, 0, 0.55),  (0.06, 0.06, 0.12), RED, 'LowerArm.R'),
    # Hands (skin)
    ('cube', (-0.35, 0, 0.44), (0.07, 0.07, 0.05), SKIN, 'Hand.L'),
    ('cube', (0.35, 0, 0.44),  (0.07, 0.07, 0.05), SKIN, 'Hand.R'),
    # Head (skin sphere)
    ('sphere', (0, 0, 1.08),   (0.18, 0.18, 0.18), SKIN, 'Head'),
    # Cap (red)
    ('sphere', (0, 0, 1.17),   (0.2, 0.2, 0.08), RED, 'Head'),
    # Cap brim
    ('cube', (0, 0.15, 1.1),   (0.14, 0.1, 0.02), RED, 'Head'),
    # Shoes (brown)
    ('cube', (-0.2, 0.05, 0.04), (0.11, 0.16, 0.04), BROWN, 'Foot.L'),
    ('cube', (0.2, 0.05, 0.04),  (0.11, 0.16, 0.04), BROWN, 'Foot.R'),
]


def create_armature():
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    armature_obj = bpy.context.active_object
    armature_obj.name = 'MarioArmature'
    armature = armature_obj.data
    armature.name = 'MarioArmature'

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
    mesh_obj.name = 'Mario'

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
    """Create all 24 animations as NLA tracks."""
    bpy.context.view_layer.objects.active = armature_obj
    if not armature_obj.animation_data:
        armature_obj.animation_data_create()

    bpy.ops.object.mode_set(mode='POSE')

    # 1. idle (48 frames, looping) — subtle breathing
    action = create_action(armature_obj, 'idle')
    reset_pose(armature_obj, 1)
    set_bone_location(armature_obj, 'Root', (0, 0, 0.01), 24)
    set_bone_rotation(armature_obj, 'Chest', (2, 0, 0), 24)
    reset_pose(armature_obj, 48)
    push_to_nla(armature_obj, action)

    # 2. walk (24 frames, looping) — walk cycle
    action = create_action(armature_obj, 'walk')
    reset_pose(armature_obj, 1)
    # Frame 1: left leg forward, right leg back
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-30, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (30, 0, 0), 1)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (15, 0, 0), 1)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (0, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-25, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperArm.L', (25, 0, 0), 1)
    # Frame 12: passing position
    reset_pose(armature_obj, 7)
    # Frame 13: right leg forward, left leg back
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-30, 0, 0), 13)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (30, 0, 0), 13)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (15, 0, 0), 13)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (0, 0, 0), 13)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-25, 0, 0), 13)
    set_bone_rotation(armature_obj, 'UpperArm.R', (25, 0, 0), 13)
    reset_pose(armature_obj, 19)
    # Loop back
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-30, 0, 0), 24)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (30, 0, 0), 24)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (15, 0, 0), 24)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-25, 0, 0), 24)
    set_bone_rotation(armature_obj, 'UpperArm.L', (25, 0, 0), 24)
    push_to_nla(armature_obj, action)

    # 3. run (16 frames, looping) — faster walk with more extreme poses
    action = create_action(armature_obj, 'run')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-45, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (40, 0, 0), 1)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (30, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-40, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperArm.L', (35, 0, 0), 1)
    set_bone_rotation(armature_obj, 'Spine', (10, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-45, 0, 0), 9)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (40, 0, 0), 9)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (30, 0, 0), 9)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-40, 0, 0), 9)
    set_bone_rotation(armature_obj, 'UpperArm.R', (35, 0, 0), 9)
    set_bone_rotation(armature_obj, 'Spine', (10, 0, 0), 9)
    # Loop
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-45, 0, 0), 16)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (40, 0, 0), 16)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (30, 0, 0), 16)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-40, 0, 0), 16)
    set_bone_rotation(armature_obj, 'UpperArm.L', (35, 0, 0), 16)
    set_bone_rotation(armature_obj, 'Spine', (10, 0, 0), 16)
    push_to_nla(armature_obj, action)

    # 4. skid (12 frames) — braking pose
    action = create_action(armature_obj, 'skid')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Spine', (-15, 0, 0), 4)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-20, 0, 10), 4)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-20, 0, -10), 4)
    set_bone_rotation(armature_obj, 'UpperArm.L', (30, 0, -20), 4)
    set_bone_rotation(armature_obj, 'UpperArm.R', (30, 0, 20), 4)
    set_bone_rotation(armature_obj, 'Spine', (-15, 0, 0), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-20, 0, 10), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-20, 0, -10), 12)
    set_bone_rotation(armature_obj, 'UpperArm.L', (30, 0, -20), 12)
    set_bone_rotation(armature_obj, 'UpperArm.R', (30, 0, 20), 12)
    push_to_nla(armature_obj, action)

    # 5. single_jump (12 frames) — jump up, arms up
    action = create_action(armature_obj, 'single_jump')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-90, 0, -30), 4)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-90, 0, 30), 4)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (15, 0, 0), 4)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (15, 0, 0), 4)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-90, 0, -30), 12)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-90, 0, 30), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (15, 0, 0), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (15, 0, 0), 12)
    push_to_nla(armature_obj, action)

    # 6. double_jump (16 frames) — forward flip
    action = create_action(armature_obj, 'double_jump')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Root', (90, 0, 0), 5)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-30, 0, 0), 5)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-30, 0, 0), 5)
    set_bone_rotation(armature_obj, 'Root', (180, 0, 0), 9)
    set_bone_rotation(armature_obj, 'Root', (270, 0, 0), 13)
    set_bone_rotation(armature_obj, 'Root', (360, 0, 0), 16)
    reset_pose(armature_obj, 16)
    push_to_nla(armature_obj, action)

    # 7. triple_jump (20 frames) — big spread spin
    action = create_action(armature_obj, 'triple_jump')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Root', (90, 0, 0), 5)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-120, 0, -60), 5)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-120, 0, 60), 5)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (20, 0, -20), 5)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (20, 0, 20), 5)
    set_bone_rotation(armature_obj, 'Root', (180, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Root', (360, 0, 0), 20)
    reset_pose(armature_obj, 20)
    push_to_nla(armature_obj, action)

    # 8. long_jump (16 frames) — horizontal dive pose
    action = create_action(armature_obj, 'long_jump')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Spine', (60, 0, 0), 4)
    set_bone_rotation(armature_obj, 'UpperArm.L', (60, 0, 0), 4)
    set_bone_rotation(armature_obj, 'UpperArm.R', (60, 0, 0), 4)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (30, 0, 0), 4)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (30, 0, 0), 4)
    set_bone_rotation(armature_obj, 'Spine', (60, 0, 0), 16)
    set_bone_rotation(armature_obj, 'UpperArm.L', (60, 0, 0), 16)
    set_bone_rotation(armature_obj, 'UpperArm.R', (60, 0, 0), 16)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (30, 0, 0), 16)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (30, 0, 0), 16)
    push_to_nla(armature_obj, action)

    # 9. backflip (20 frames) — backward flip
    action = create_action(armature_obj, 'backflip')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Root', (-90, 0, 0), 5)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-60, 0, -40), 5)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-60, 0, 40), 5)
    set_bone_rotation(armature_obj, 'Root', (-180, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Root', (-270, 0, 0), 15)
    set_bone_rotation(armature_obj, 'Root', (-360, 0, 0), 20)
    reset_pose(armature_obj, 20)
    push_to_nla(armature_obj, action)

    # 10. side_somersault (20 frames) — sideways flip
    action = create_action(armature_obj, 'side_somersault')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Root', (0, 90, 0), 5)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-90, 0, -30), 5)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-90, 0, 30), 5)
    set_bone_rotation(armature_obj, 'Root', (0, 180, 0), 10)
    set_bone_rotation(armature_obj, 'Root', (0, 270, 0), 15)
    set_bone_rotation(armature_obj, 'Root', (0, 360, 0), 20)
    reset_pose(armature_obj, 20)
    push_to_nla(armature_obj, action)

    # 11. freefall (12 frames, looping) — falling arms out
    action = create_action(armature_obj, 'freefall')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-60, 0, -60), 3)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-60, 0, 60), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (10, 0, -10), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (10, 0, 10), 3)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-50, 0, -50), 9)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-50, 0, 50), 9)
    # Loop point
    set_bone_rotation(armature_obj, 'UpperArm.L', (-60, 0, -60), 12)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-60, 0, 60), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (10, 0, -10), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (10, 0, 10), 12)
    push_to_nla(armature_obj, action)

    # 12. ground_pound_spin (8 frames) — quick spin
    action = create_action(armature_obj, 'ground_pound_spin')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Root', (0, 0, 180), 4)
    set_bone_rotation(armature_obj, 'Root', (0, 0, 360), 8)
    reset_pose(armature_obj, 8)
    push_to_nla(armature_obj, action)

    # 13. ground_pound_fall (6 frames) — curled ball falling
    action = create_action(armature_obj, 'ground_pound_fall')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Spine', (30, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-60, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-60, 0, 0), 3)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (60, 0, 0), 3)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (60, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperArm.L', (40, 0, 30), 3)
    set_bone_rotation(armature_obj, 'UpperArm.R', (40, 0, -30), 3)
    # Hold pose
    set_bone_rotation(armature_obj, 'Spine', (30, 0, 0), 6)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-60, 0, 0), 6)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-60, 0, 0), 6)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (60, 0, 0), 6)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (60, 0, 0), 6)
    set_bone_rotation(armature_obj, 'UpperArm.L', (40, 0, 30), 6)
    set_bone_rotation(armature_obj, 'UpperArm.R', (40, 0, -30), 6)
    push_to_nla(armature_obj, action)

    # 14. ground_pound_land (10 frames) — impact
    action = create_action(armature_obj, 'ground_pound_land')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Spine', (-10, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-30, 0, 10), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-30, 0, -10), 3)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (30, 0, 0), 3)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (30, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperArm.L', (30, 0, -40), 3)
    set_bone_rotation(armature_obj, 'UpperArm.R', (30, 0, 40), 3)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.05), 3)
    reset_pose(armature_obj, 10)
    push_to_nla(armature_obj, action)

    # 15. crouch (6 frames) — squat down
    action = create_action(armature_obj, 'crouch')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-50, 0, 10), 4)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-50, 0, -10), 4)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (50, 0, 0), 4)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (50, 0, 0), 4)
    set_bone_rotation(armature_obj, 'Spine', (20, 0, 0), 4)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.15), 4)
    # Hold
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-50, 0, 10), 6)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-50, 0, -10), 6)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (50, 0, 0), 6)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (50, 0, 0), 6)
    set_bone_rotation(armature_obj, 'Spine', (20, 0, 0), 6)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.15), 6)
    push_to_nla(armature_obj, action)

    # 16. crawl (24 frames, looping) — crawling cycle
    action = create_action(armature_obj, 'crawl')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Spine', (60, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-40, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (0, 0, 0), 1)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (40, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperArm.L', (60, 0, 0), 1)
    set_bone_rotation(armature_obj, 'UpperArm.R', (30, 0, 0), 1)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.2), 1)

    set_bone_rotation(armature_obj, 'Spine', (60, 0, 0), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-40, 0, 0), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (0, 0, 0), 12)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (40, 0, 0), 12)
    set_bone_rotation(armature_obj, 'UpperArm.R', (60, 0, 0), 12)
    set_bone_rotation(armature_obj, 'UpperArm.L', (30, 0, 0), 12)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.2), 12)

    # Loop back
    set_bone_rotation(armature_obj, 'Spine', (60, 0, 0), 24)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-40, 0, 0), 24)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (0, 0, 0), 24)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (40, 0, 0), 24)
    set_bone_rotation(armature_obj, 'UpperArm.L', (60, 0, 0), 24)
    set_bone_rotation(armature_obj, 'UpperArm.R', (30, 0, 0), 24)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.2), 24)
    push_to_nla(armature_obj, action)

    # 17. punch_1 (8 frames) — left jab
    action = create_action(armature_obj, 'punch_1')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'UpperArm.L', (60, 0, 0), 3)
    set_bone_rotation(armature_obj, 'LowerArm.L', (-30, 0, 0), 3)
    set_bone_rotation(armature_obj, 'Spine', (0, 0, 15), 3)
    reset_pose(armature_obj, 8)
    push_to_nla(armature_obj, action)

    # 18. punch_2 (8 frames) — right jab
    action = create_action(armature_obj, 'punch_2')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'UpperArm.R', (60, 0, 0), 3)
    set_bone_rotation(armature_obj, 'LowerArm.R', (-30, 0, 0), 3)
    set_bone_rotation(armature_obj, 'Spine', (0, 0, -15), 3)
    reset_pose(armature_obj, 8)
    push_to_nla(armature_obj, action)

    # 19. punch_3_kick (10 frames) — spinning kick
    action = create_action(armature_obj, 'punch_3_kick')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Root', (0, 0, 90), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (60, 0, 0), 3)
    set_bone_rotation(armature_obj, 'Root', (0, 0, 180), 5)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (60, 0, 0), 5)
    set_bone_rotation(armature_obj, 'Root', (0, 0, 360), 10)
    reset_pose(armature_obj, 10)
    push_to_nla(armature_obj, action)

    # 20. jump_kick (10 frames) — mid-air kick
    action = create_action(armature_obj, 'jump_kick')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (70, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-20, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-30, 0, -20), 3)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-30, 0, 20), 3)
    set_bone_rotation(armature_obj, 'Spine', (-10, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (70, 0, 0), 10)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-20, 0, 0), 10)
    set_bone_rotation(armature_obj, 'UpperArm.L', (-30, 0, -20), 10)
    set_bone_rotation(armature_obj, 'UpperArm.R', (-30, 0, 20), 10)
    set_bone_rotation(armature_obj, 'Spine', (-10, 0, 0), 10)
    push_to_nla(armature_obj, action)

    # 21. dive (10 frames) — forward dive
    action = create_action(armature_obj, 'dive')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Spine', (70, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperArm.L', (80, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperArm.R', (80, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (20, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (20, 0, 0), 3)
    # Hold
    set_bone_rotation(armature_obj, 'Spine', (70, 0, 0), 10)
    set_bone_rotation(armature_obj, 'UpperArm.L', (80, 0, 0), 10)
    set_bone_rotation(armature_obj, 'UpperArm.R', (80, 0, 0), 10)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (20, 0, 0), 10)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (20, 0, 0), 10)
    push_to_nla(armature_obj, action)

    # 22. belly_slide (12 frames, looping) — sliding on belly
    action = create_action(armature_obj, 'belly_slide')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Spine', (80, 0, 0), 2)
    set_bone_rotation(armature_obj, 'UpperArm.L', (30, 0, -30), 2)
    set_bone_rotation(armature_obj, 'UpperArm.R', (30, 0, 30), 2)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (10, 0, -10), 2)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (10, 0, 10), 2)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.3), 2)
    # Slight wobble
    set_bone_rotation(armature_obj, 'Spine', (80, 0, 3), 7)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.28), 7)
    # Loop
    set_bone_rotation(armature_obj, 'Spine', (80, 0, 0), 12)
    set_bone_rotation(armature_obj, 'UpperArm.L', (30, 0, -30), 12)
    set_bone_rotation(armature_obj, 'UpperArm.R', (30, 0, 30), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (10, 0, -10), 12)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (10, 0, 10), 12)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.3), 12)
    push_to_nla(armature_obj, action)

    # 23. slide_kick (10 frames) — low sliding kick
    action = create_action(armature_obj, 'slide_kick')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'Spine', (40, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-20, 0, 0), 3)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (70, 0, 0), 3)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (40, 0, 0), 3)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.2), 3)
    # Hold
    set_bone_rotation(armature_obj, 'Spine', (40, 0, 0), 10)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-20, 0, 0), 10)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (70, 0, 0), 10)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (40, 0, 0), 10)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.2), 10)
    push_to_nla(armature_obj, action)

    # 24. landing (6 frames) — impact absorb
    action = create_action(armature_obj, 'landing')
    reset_pose(armature_obj, 1)
    set_bone_rotation(armature_obj, 'UpperLeg.L', (-25, 0, 5), 2)
    set_bone_rotation(armature_obj, 'UpperLeg.R', (-25, 0, -5), 2)
    set_bone_rotation(armature_obj, 'LowerLeg.L', (25, 0, 0), 2)
    set_bone_rotation(armature_obj, 'LowerLeg.R', (25, 0, 0), 2)
    set_bone_rotation(armature_obj, 'Spine', (10, 0, 0), 2)
    set_bone_location(armature_obj, 'Root', (0, 0, -0.08), 2)
    reset_pose(armature_obj, 6)
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
    out_path = os.path.join(project_root, "resources", "models", "mario.glb")

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
