"""Generate Bob-omb model with armature and animations.
Output: resources/models/bob_omb.glb
Run: blender --background --python tools/models/bob_omb.py

Blender Z-up. glTF exporter converts to Y-up automatically.
Bob-omb is ~0.8 units tall, centered at origin, facing +Y (becomes +Z in engine).
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

BLACK = (0.08, 0.08, 0.08)
DARK_GRAY = (0.3, 0.3, 0.3)
YELLOW = (1.0, 0.85, 0.0)
TAN = (0.85, 0.7, 0.45)
WHITE = (0.95, 0.95, 0.95)

# ---- Bone Dimensions (Blender Z-up) ----

BONE_DEFS = {
    'Root':   {'head': (0, 0, 0),         'tail': (0, 0, 0.35),    'parent': None},
    'Body':   {'head': (0, 0, 0.25),      'tail': (0, 0, 0.65),    'parent': 'Root'},
    'Fuse':   {'head': (0, 0, 0.65),      'tail': (0, 0, 0.85),    'parent': 'Body'},
    'Foot.L': {'head': (-0.15, 0, 0),     'tail': (-0.15, 0.1, 0), 'parent': 'Root'},
    'Foot.R': {'head': (0.15, 0, 0),      'tail': (0.15, 0.1, 0),  'parent': 'Root'},
    'Key':    {'head': (0, -0.25, 0.45),  'tail': (0, -0.35, 0.45),'parent': 'Body'},
}

# Ordered so parents always come before children
BONE_ORDER = [
    'Root', 'Body', 'Fuse', 'Foot.L', 'Foot.R', 'Key',
]

# ---- Body Part Definitions ----
# Each: (primitive, location, scale, color, vertex_group)

BODY_PARTS = [
    # Main bomb body — sphere, black
    ('sphere', (0, 0, 0.4), (0.25, 0.25, 0.25), BLACK, 'Body'),
    # Fuse stem — small cylinder, dark gray
    ('cube', (0, 0, 0.7), (0.04, 0.04, 0.08), DARK_GRAY, 'Fuse'),
    # Fuse tip — yellow
    ('sphere', (0, 0, 0.79), (0.03, 0.03, 0.03), YELLOW, 'Fuse'),
    # Left foot — oval, tan
    ('cube', (-0.12, 0.04, 0.04), (0.08, 0.12, 0.04), TAN, 'Foot.L'),
    # Right foot — oval, tan
    ('cube', (0.12, 0.04, 0.04), (0.08, 0.12, 0.04), TAN, 'Foot.R'),
    # Left eye — white circle
    ('sphere', (-0.1, 0.2, 0.45), (0.06, 0.03, 0.06), WHITE, 'Body'),
    # Right eye — white circle
    ('sphere', (0.1, 0.2, 0.45), (0.06, 0.03, 0.06), WHITE, 'Body'),
    # Winding key — cross shape (horizontal bar)
    ('cube', (0, -0.32, 0.45), (0.08, 0.02, 0.02), YELLOW, 'Key'),
    # Winding key — cross shape (vertical bar)
    ('cube', (0, -0.32, 0.45), (0.02, 0.02, 0.08), YELLOW, 'Key'),
]


def create_armature():
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    armature_obj = bpy.context.active_object
    armature_obj.name = 'BobOmbArmature'
    armature = armature_obj.data
    armature.name = 'BobOmbArmature'

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
    mesh_obj.name = 'BobOmb'

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


def set_bone_scale(armature_obj, bone_name, scale, frame):
    """Set a bone's scale at a given frame."""
    bone = armature_obj.pose.bones.get(bone_name)
    if not bone:
        return
    bone.scale = Vector(scale)
    bone.keyframe_insert(data_path="scale", frame=frame)


def reset_pose(armature_obj, frame):
    """Key all bones at rest pose for the given frame."""
    for bone in armature_obj.pose.bones:
        bone.rotation_mode = 'XYZ'
        bone.rotation_euler = Euler((0, 0, 0))
        bone.location = Vector((0, 0, 0))
        bone.scale = Vector((1, 1, 1))
        bone.keyframe_insert(data_path="rotation_euler", frame=frame)
        bone.keyframe_insert(data_path="location", frame=frame)
        bone.keyframe_insert(data_path="scale", frame=frame)


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

    # 1. walk (16 frames, looping) — feet waddle, key turns slowly, body bobs
    action = create_action(armature_obj, 'walk')
    reset_pose(armature_obj, 1)
    # Frame 1: left foot forward, right foot back
    set_bone_rotation(armature_obj, 'Foot.L', (15, 0, -10), 1)
    set_bone_rotation(armature_obj, 'Foot.R', (-15, 0, 10), 1)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.02), 1)
    set_bone_rotation(armature_obj, 'Body', (0, 0, -3), 1)
    set_bone_rotation(armature_obj, 'Key', (0, 0, 0), 1)
    # Frame 5: passing, body up
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, 0), 5)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 0), 5)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.04), 5)
    set_bone_rotation(armature_obj, 'Body', (0, 0, 0), 5)
    set_bone_rotation(armature_obj, 'Key', (0, 90, 0), 5)
    # Frame 9: right foot forward, left foot back
    set_bone_rotation(armature_obj, 'Foot.L', (-15, 0, 10), 9)
    set_bone_rotation(armature_obj, 'Foot.R', (15, 0, -10), 9)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.02), 9)
    set_bone_rotation(armature_obj, 'Body', (0, 0, 3), 9)
    set_bone_rotation(armature_obj, 'Key', (0, 180, 0), 9)
    # Frame 13: passing, body up
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, 0), 13)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 0), 13)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.04), 13)
    set_bone_rotation(armature_obj, 'Body', (0, 0, 0), 13)
    set_bone_rotation(armature_obj, 'Key', (0, 270, 0), 13)
    # Frame 16: loop back
    set_bone_rotation(armature_obj, 'Foot.L', (15, 0, -10), 16)
    set_bone_rotation(armature_obj, 'Foot.R', (-15, 0, 10), 16)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.02), 16)
    set_bone_rotation(armature_obj, 'Body', (0, 0, -3), 16)
    set_bone_rotation(armature_obj, 'Key', (0, 360, 0), 16)
    push_to_nla(armature_obj, action)

    # 2. chase (12 frames, looping) — faster waddle, body leans forward, key turns faster
    action = create_action(armature_obj, 'chase')
    reset_pose(armature_obj, 1)
    # Frame 1: left foot forward
    set_bone_rotation(armature_obj, 'Foot.L', (25, 0, -15), 1)
    set_bone_rotation(armature_obj, 'Foot.R', (-25, 0, 15), 1)
    set_bone_rotation(armature_obj, 'Body', (18, 0, -5), 1)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.01), 1)
    set_bone_rotation(armature_obj, 'Key', (0, 0, 0), 1)
    # Frame 4: passing
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, 0), 4)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 0), 4)
    set_bone_rotation(armature_obj, 'Body', (18, 0, 0), 4)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.03), 4)
    set_bone_rotation(armature_obj, 'Key', (0, 120, 0), 4)
    # Frame 7: right foot forward
    set_bone_rotation(armature_obj, 'Foot.L', (-25, 0, 15), 7)
    set_bone_rotation(armature_obj, 'Foot.R', (25, 0, -15), 7)
    set_bone_rotation(armature_obj, 'Body', (18, 0, 5), 7)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.01), 7)
    set_bone_rotation(armature_obj, 'Key', (0, 240, 0), 7)
    # Frame 10: passing
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Body', (18, 0, 0), 10)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.03), 10)
    set_bone_rotation(armature_obj, 'Key', (0, 360, 0), 10)
    # Frame 12: loop back
    set_bone_rotation(armature_obj, 'Foot.L', (25, 0, -15), 12)
    set_bone_rotation(armature_obj, 'Foot.R', (-25, 0, 15), 12)
    set_bone_rotation(armature_obj, 'Body', (18, 0, -5), 12)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.01), 12)
    set_bone_rotation(armature_obj, 'Key', (0, 480, 0), 12)
    push_to_nla(armature_obj, action)

    # 3. fuse_burning (8 frames, looping) — body shakes, fuse shortens
    action = create_action(armature_obj, 'fuse_burning')
    reset_pose(armature_obj, 1)
    # Frame 1: slight shake right, fuse starts shortening
    set_bone_rotation(armature_obj, 'Body', (2, 3, 2), 1)
    set_bone_scale(armature_obj, 'Fuse', (1, 1, 1), 1)
    # Frame 2: shake left
    set_bone_rotation(armature_obj, 'Body', (-3, -2, -3), 2)
    set_bone_scale(armature_obj, 'Fuse', (1, 1, 0.88), 2)
    # Frame 3: shake right
    set_bone_rotation(armature_obj, 'Body', (2, 2, 3), 3)
    set_bone_scale(armature_obj, 'Fuse', (1, 1, 0.75), 3)
    # Frame 4: shake left
    set_bone_rotation(armature_obj, 'Body', (-2, -3, -2), 4)
    set_bone_scale(armature_obj, 'Fuse', (1, 1, 0.63), 4)
    # Frame 5: shake right
    set_bone_rotation(armature_obj, 'Body', (3, 2, 2), 5)
    set_bone_scale(armature_obj, 'Fuse', (1, 1, 0.5), 5)
    # Frame 6: shake left
    set_bone_rotation(armature_obj, 'Body', (-2, -2, -3), 6)
    set_bone_scale(armature_obj, 'Fuse', (1, 1, 0.38), 6)
    # Frame 7: shake right
    set_bone_rotation(armature_obj, 'Body', (2, 3, 2), 7)
    set_bone_scale(armature_obj, 'Fuse', (1, 1, 0.25), 7)
    # Frame 8: loop back — shake left, fuse very short
    set_bone_rotation(armature_obj, 'Body', (-3, -2, -2), 8)
    set_bone_scale(armature_obj, 'Fuse', (1, 1, 0.12), 8)
    push_to_nla(armature_obj, action)

    # 4. explode (10 frames, one-shot) — body scales up 1.5x, then to 0
    action = create_action(armature_obj, 'explode')
    reset_pose(armature_obj, 1)
    # Frame 1-5: body swells up
    set_bone_scale(armature_obj, 'Body', (1.0, 1.0, 1.0), 1)
    set_bone_scale(armature_obj, 'Fuse', (0, 0, 0), 1)
    set_bone_scale(armature_obj, 'Body', (1.15, 1.15, 1.15), 2)
    set_bone_rotation(armature_obj, 'Body', (2, -2, 3), 2)
    set_bone_scale(armature_obj, 'Body', (1.25, 1.25, 1.25), 3)
    set_bone_rotation(armature_obj, 'Body', (-3, 2, -2), 3)
    set_bone_scale(armature_obj, 'Body', (1.4, 1.4, 1.4), 4)
    set_bone_rotation(armature_obj, 'Body', (2, 3, 2), 4)
    set_bone_scale(armature_obj, 'Body', (1.5, 1.5, 1.5), 5)
    set_bone_rotation(armature_obj, 'Body', (0, 0, 0), 5)
    # Frame 6-10: body collapses to nothing
    set_bone_scale(armature_obj, 'Body', (1.2, 1.2, 1.2), 6)
    set_bone_scale(armature_obj, 'Body', (0.8, 0.8, 0.8), 7)
    set_bone_scale(armature_obj, 'Foot.L', (0.5, 0.5, 0.5), 7)
    set_bone_scale(armature_obj, 'Foot.R', (0.5, 0.5, 0.5), 7)
    set_bone_scale(armature_obj, 'Body', (0.4, 0.4, 0.4), 8)
    set_bone_scale(armature_obj, 'Foot.L', (0.2, 0.2, 0.2), 8)
    set_bone_scale(armature_obj, 'Foot.R', (0.2, 0.2, 0.2), 8)
    set_bone_scale(armature_obj, 'Key', (0, 0, 0), 8)
    set_bone_scale(armature_obj, 'Body', (0.1, 0.1, 0.1), 9)
    set_bone_scale(armature_obj, 'Foot.L', (0.05, 0.05, 0.05), 9)
    set_bone_scale(armature_obj, 'Foot.R', (0.05, 0.05, 0.05), 9)
    set_bone_scale(armature_obj, 'Body', (0, 0, 0), 10)
    set_bone_scale(armature_obj, 'Foot.L', (0, 0, 0), 10)
    set_bone_scale(armature_obj, 'Foot.R', (0, 0, 0), 10)
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
    out_path = os.path.join(project_root, "resources", "models", "bob_omb.glb")

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
