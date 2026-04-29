"""Generate a simple placeholder Mario model with vertex colors.
Output: resources/models/mario.glb
Run: blender --background --python tools/models/mario.py

Blender Z-up. glTF exporter converts to Y-up.
Mario is ~1.5 units tall, centered at origin, facing +Y (which becomes +Z in engine).
"""
import bpy
import bmesh
import os

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()

def set_vertex_colors(obj, color):
    mesh = obj.data
    if not mesh.color_attributes:
        mesh.color_attributes.new(name="Color", type='FLOAT_COLOR', domain='CORNER')
    color_attr = mesh.color_attributes[0]
    for i in range(len(color_attr.data)):
        color_attr.data[i].color = (*color, 1.0)

def main():
    clear_scene()

    RED = (0.9, 0.15, 0.1)
    BLUE = (0.15, 0.15, 0.8)
    SKIN = (1.0, 0.8, 0.6)
    BROWN = (0.45, 0.25, 0.1)

    # Legs (blue) — two boxes
    for side in [-0.2, 0.2]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(side, 0, 0.25))
        leg = bpy.context.active_object
        leg.scale = (0.15, 0.15, 0.25)
        set_vertex_colors(leg, BLUE)

    # Torso (red) — box
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.7))
    torso = bpy.context.active_object
    torso.scale = (0.25, 0.2, 0.2)
    set_vertex_colors(torso, RED)

    # Arms (red) — two boxes
    for side in [-0.35, 0.35]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(side, 0, 0.7))
        arm = bpy.context.active_object
        arm.scale = (0.1, 0.1, 0.2)
        set_vertex_colors(arm, RED)

    # Hands (skin) — two small cubes
    for side in [-0.35, 0.35]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(side, 0, 0.48))
        hand = bpy.context.active_object
        hand.scale = (0.08, 0.08, 0.06)
        set_vertex_colors(hand, SKIN)

    # Head (skin) — sphere
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=8, radius=0.18, location=(0, 0, 1.05))
    head = bpy.context.active_object
    set_vertex_colors(head, SKIN)

    # Cap (red) — flattened hemisphere on top of head
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=8, radius=0.2, location=(0, 0, 1.15))
    cap = bpy.context.active_object
    cap.scale = (1, 1, 0.4)
    set_vertex_colors(cap, RED)

    # Cap brim — thin box in front
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.15, 1.08))
    brim = bpy.context.active_object
    brim.scale = (0.15, 0.1, 0.02)
    set_vertex_colors(brim, RED)

    # Shoes (brown) — two boxes
    for side in [-0.2, 0.2]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(side, 0.05, 0.05))
        shoe = bpy.context.active_object
        shoe.scale = (0.13, 0.18, 0.05)
        set_vertex_colors(shoe, BROWN)

    # Apply transforms and join
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.context.view_layer.objects.active = bpy.context.selected_objects[0]
    bpy.ops.object.join()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, "..", ".."))
    out_path = os.path.join(project_root, "resources", "models", "mario.glb")

    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format='GLB',
    )
    print(f"Exported: {out_path}")

if __name__ == "__main__":
    main()
