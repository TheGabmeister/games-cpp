"""Generate a simple test scene with vertex-colored shapes for Phase 1 pipeline validation.
Output: resources/models/test_scene.glb
Run: blender --background --python tools/models/test_scene.py
"""
import bpy
import bmesh
import os
import math

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    for col in bpy.data.collections:
        if col != bpy.context.scene.collection:
            bpy.data.collections.remove(col)

def set_vertex_colors(obj, color):
    mesh = obj.data
    if not mesh.color_attributes:
        mesh.color_attributes.new(name="Color", type='FLOAT_COLOR', domain='CORNER')
    color_attr = mesh.color_attributes[0]
    for i in range(len(color_attr.data)):
        color_attr.data[i].color = (*color, 1.0)

def create_ramp():
    bm = bmesh.new()
    verts = [
        bm.verts.new((-2, 0, 0)),
        bm.verts.new((2, 0, 0)),
        bm.verts.new((2, 0, -4)),
        bm.verts.new((-2, 0, -4)),
        bm.verts.new((-2, 2, -4)),
        bm.verts.new((2, 2, -4)),
    ]
    bm.faces.new([verts[0], verts[1], verts[2], verts[3]])  # bottom
    bm.faces.new([verts[0], verts[3], verts[4]])             # left tri
    bm.faces.new([verts[1], verts[5], verts[2]])             # right tri
    bm.faces.new([verts[0], verts[4], verts[5], verts[1]])   # slope
    bm.faces.new([verts[3], verts[2], verts[5], verts[4]])   # back wall

    mesh = bpy.data.meshes.new("Ramp")
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new("Ramp", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = (8, 0, 0)
    set_vertex_colors(obj, (0.8, 0.5, 0.2))
    return obj

def create_cylinder():
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=16, radius=1, depth=3, location=(0, 1.5, -8)
    )
    obj = bpy.context.active_object
    obj.name = "Cylinder"
    set_vertex_colors(obj, (0.6, 0.2, 0.7))
    return obj

def create_steps():
    bm = bmesh.new()

    step_width = 4
    step_depth = 1.5
    step_height = 0.5
    num_steps = 4

    for i in range(num_steps):
        x0 = -step_width / 2
        x1 = step_width / 2
        y0 = i * step_height
        y1 = (i + 1) * step_height
        z0 = -i * step_depth
        z1 = -(i + 1) * step_depth

        v = [
            bm.verts.new((x0, y0, z0)),
            bm.verts.new((x1, y0, z0)),
            bm.verts.new((x1, y1, z0)),
            bm.verts.new((x0, y1, z0)),
            bm.verts.new((x0, y0, z1)),
            bm.verts.new((x1, y0, z1)),
            bm.verts.new((x1, y1, z1)),
            bm.verts.new((x0, y1, z1)),
        ]
        # front
        bm.faces.new([v[0], v[1], v[2], v[3]])
        # top
        bm.faces.new([v[3], v[2], v[6], v[7]])
        # left
        bm.faces.new([v[0], v[3], v[7], v[4]])
        # right
        bm.faces.new([v[1], v[5], v[6], v[2]])
        # bottom
        bm.faces.new([v[0], v[4], v[5], v[1]])
        # back
        bm.faces.new([v[4], v[7], v[6], v[5]])

    mesh = bpy.data.meshes.new("Steps")
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new("Steps", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = (-8, 0, -3)
    set_vertex_colors(obj, (0.3, 0.7, 0.4))
    return obj

def main():
    clear_scene()

    ramp = create_ramp()
    cylinder = create_cylinder()
    steps = create_steps()

    # Join all objects into one mesh for single-primitive export
    bpy.ops.object.select_all(action='SELECT')
    bpy.context.view_layer.objects.active = ramp
    bpy.ops.object.join()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, "..", ".."))
    out_path = os.path.join(project_root, "resources", "models", "test_scene.glb")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format='GLB',
    )
    print(f"Exported: {out_path}")

if __name__ == "__main__":
    main()
