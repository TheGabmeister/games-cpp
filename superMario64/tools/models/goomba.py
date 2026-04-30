"""Generate Goomba model with armature and animations.
Output: resources/models/goomba.glb
Run: blender --background --python tools/models/goomba.py

Blender Z-up. glTF exporter converts to Y-up automatically.
Goomba is ~0.8 units tall, centered at origin, facing +Y (becomes +Z in engine).
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

BROWN = (0.55, 0.35, 0.15)
TAN = (0.85, 0.7, 0.45)
DARK_BROWN = (0.3, 0.15, 0.05)
BLACK = (0.1, 0.1, 0.1)
WHITE = (0.95, 0.95, 0.95)

# ---- Bone Dimensions (Blender Z-up) ----

BONE_DEFS = {
    'Root':     {'head': (0, 0, 0),       'tail': (0, 0, 0.4),   'parent': None},
    'Body':     {'head': (0, 0, 0.2),     'tail': (0, 0, 0.7),   'parent': 'Root'},
    'Foot.L':   {'head': (-0.15, 0, 0),   'tail': (-0.15, 0.1, 0), 'parent': 'Root'},
    'Foot.R':   {'head': (0.15, 0, 0),    'tail': (0.15, 0.1, 0),  'parent': 'Root'},
    'Eyebrows': {'head': (0, 0.15, 0.6),  'tail': (0, 0.2, 0.65),  'parent': 'Body'},
}

# Ordered so parents always come before children
BONE_ORDER = [
    'Root', 'Body', 'Foot.L', 'Foot.R', 'Eyebrows',
]

# ---- Body Part Definitions ----
# Each: (primitive, location, scale, color, vertex_group)

BODY_PARTS = [
    # Main body — flattened sphere (wider than tall), brown
    ('sphere', (0, 0, 0.45), (0.28, 0.25, 0.22), BROWN, 'Body'),
    # Underbelly — lighter tan disc at bottom of body
    ('sphere', (0, 0, 0.28), (0.24, 0.22, 0.08), TAN, 'Body'),
    # Left foot — small oval, dark brown
    ('cube', (-0.12, 0.04, 0.04), (0.08, 0.12, 0.04), DARK_BROWN, 'Foot.L'),
    # Right foot — small oval, dark brown
    ('cube', (0.12, 0.04, 0.04), (0.08, 0.12, 0.04), DARK_BROWN, 'Foot.R'),
    # Left eyebrow ridge — angry slant, black
    ('cube', (-0.08, 0.2, 0.58), (0.08, 0.02, 0.025), BLACK, 'Eyebrows'),
    # Right eyebrow ridge — angry slant, black
    ('cube', (0.08, 0.2, 0.58), (0.08, 0.02, 0.025), BLACK, 'Eyebrows'),
    # Fangs/teeth area — white strip at front
    ('cube', (0, 0.22, 0.42), (0.1, 0.02, 0.04), WHITE, 'Body'),
]


def create_armature():
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    armature_obj = bpy.context.active_object
    armature_obj.name = 'GoombaArmature'
    armature = armature_obj.data
    armature.name = 'GoombaArmature'

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
    mesh_obj.name = 'Goomba'

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

    # 1. walk (16 frames, looping) — feet waddle side to side, body bobs
    action = create_action(armature_obj, 'walk')
    reset_pose(armature_obj, 1)
    # Frame 1: left foot forward, right foot back
    set_bone_rotation(armature_obj, 'Foot.L', (15, 0, -10), 1)
    set_bone_rotation(armature_obj, 'Foot.R', (-15, 0, 10), 1)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.02), 1)
    set_bone_rotation(armature_obj, 'Body', (0, 0, -5), 1)
    # Frame 5: passing, body up
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, 0), 5)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 0), 5)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.04), 5)
    set_bone_rotation(armature_obj, 'Body', (0, 0, 0), 5)
    # Frame 9: right foot forward, left foot back
    set_bone_rotation(armature_obj, 'Foot.L', (-15, 0, 10), 9)
    set_bone_rotation(armature_obj, 'Foot.R', (15, 0, -10), 9)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.02), 9)
    set_bone_rotation(armature_obj, 'Body', (0, 0, 5), 9)
    # Frame 13: passing, body up
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, 0), 13)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 0), 13)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.04), 13)
    set_bone_rotation(armature_obj, 'Body', (0, 0, 0), 13)
    # Frame 16: loop back to start
    set_bone_rotation(armature_obj, 'Foot.L', (15, 0, -10), 16)
    set_bone_rotation(armature_obj, 'Foot.R', (-15, 0, 10), 16)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.02), 16)
    set_bone_rotation(armature_obj, 'Body', (0, 0, -5), 16)
    push_to_nla(armature_obj, action)

    # 2. charge (12 frames, looping) — faster waddle, body tilts forward, angrier eyebrows
    action = create_action(armature_obj, 'charge')
    reset_pose(armature_obj, 1)
    # Frame 1: left foot forward, body leaning
    set_bone_rotation(armature_obj, 'Foot.L', (25, 0, -15), 1)
    set_bone_rotation(armature_obj, 'Foot.R', (-25, 0, 15), 1)
    set_bone_rotation(armature_obj, 'Body', (20, 0, -8), 1)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.01), 1)
    set_bone_rotation(armature_obj, 'Eyebrows', (-15, 0, 0), 1)
    # Frame 4: passing
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, 0), 4)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 0), 4)
    set_bone_rotation(armature_obj, 'Body', (20, 0, 0), 4)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.03), 4)
    set_bone_rotation(armature_obj, 'Eyebrows', (-15, 0, 0), 4)
    # Frame 7: right foot forward
    set_bone_rotation(armature_obj, 'Foot.L', (-25, 0, 15), 7)
    set_bone_rotation(armature_obj, 'Foot.R', (25, 0, -15), 7)
    set_bone_rotation(armature_obj, 'Body', (20, 0, 8), 7)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.01), 7)
    set_bone_rotation(armature_obj, 'Eyebrows', (-15, 0, 0), 7)
    # Frame 10: passing
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 0), 10)
    set_bone_rotation(armature_obj, 'Body', (20, 0, 0), 10)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.03), 10)
    set_bone_rotation(armature_obj, 'Eyebrows', (-15, 0, 0), 10)
    # Frame 12: loop back
    set_bone_rotation(armature_obj, 'Foot.L', (25, 0, -15), 12)
    set_bone_rotation(armature_obj, 'Foot.R', (-25, 0, 15), 12)
    set_bone_rotation(armature_obj, 'Body', (20, 0, -8), 12)
    set_bone_location(armature_obj, 'Body', (0, 0, 0.01), 12)
    set_bone_rotation(armature_obj, 'Eyebrows', (-15, 0, 0), 12)
    push_to_nla(armature_obj, action)

    # 3. squished (8 frames, one-shot) — body flattens on Z, feet splay outward
    action = create_action(armature_obj, 'squished')
    reset_pose(armature_obj, 1)
    # Frame 3: partially squished
    set_bone_scale(armature_obj, 'Body', (1.3, 1.3, 0.4), 3)
    set_bone_location(armature_obj, 'Body', (0, 0, -0.15), 3)
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, -25), 3)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 25), 3)
    set_bone_location(armature_obj, 'Foot.L', (-0.06, 0, 0), 3)
    set_bone_location(armature_obj, 'Foot.R', (0.06, 0, 0), 3)
    # Frame 5: fully squished
    set_bone_scale(armature_obj, 'Body', (1.5, 1.5, 0.15), 5)
    set_bone_location(armature_obj, 'Body', (0, 0, -0.25), 5)
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, -40), 5)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 40), 5)
    set_bone_location(armature_obj, 'Foot.L', (-0.1, 0, 0), 5)
    set_bone_location(armature_obj, 'Foot.R', (0.1, 0, 0), 5)
    # Frame 8: hold squished
    set_bone_scale(armature_obj, 'Body', (1.5, 1.5, 0.15), 8)
    set_bone_location(armature_obj, 'Body', (0, 0, -0.25), 8)
    set_bone_rotation(armature_obj, 'Foot.L', (0, 0, -40), 8)
    set_bone_rotation(armature_obj, 'Foot.R', (0, 0, 40), 8)
    set_bone_location(armature_obj, 'Foot.L', (-0.1, 0, 0), 8)
    set_bone_location(armature_obj, 'Foot.R', (0.1, 0, 0), 8)
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
    out_path = os.path.join(project_root, "resources", "models", "goomba.glb")

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
