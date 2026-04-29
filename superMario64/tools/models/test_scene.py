"""Generate a simple test scene with vertex-colored shapes for Phase 1 pipeline validation.
Output: resources/models/test_scene.glb
Run: blender --background --python tools/models/test_scene.py

Note: Blender is Z-up. The glTF exporter converts to Y-up automatically.
So in this script, Z = vertical height.
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

def create_ramp():
    bm = bmesh.new()
    # Z is up in Blender. Ramp slopes upward along -Y.
    verts = [
        bm.verts.new((-2, 0, 0)),    # front-left, ground
        bm.verts.new((2, 0, 0)),     # front-right, ground
        bm.verts.new((2, -4, 0)),    # back-right, ground
        bm.verts.new((-2, -4, 0)),   # back-left, ground
        bm.verts.new((-2, -4, 2)),   # back-left, top
        bm.verts.new((2, -4, 2)),    # back-right, top
    ]
    bm.faces.new([verts[3], verts[2], verts[1], verts[0]])   # bottom
    bm.faces.new([verts[0], verts[3], verts[4]])              # left tri
    bm.faces.new([verts[5], verts[2], verts[1]])              # right tri
    bm.faces.new([verts[1], verts[0], verts[4], verts[5]])    # slope
    bm.faces.new([verts[4], verts[3], verts[2], verts[5]])    # back wall

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
        vertices=16, radius=1, depth=3, location=(0, -8, 1.5)
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
        z0 = i * step_height
        z1 = (i + 1) * step_height
        y0 = -i * step_depth
        y1 = -(i + 1) * step_depth

        v = [
            bm.verts.new((x0, y0, z0)),  # 0: front-left-bottom
            bm.verts.new((x1, y0, z0)),  # 1: front-right-bottom
            bm.verts.new((x1, y0, z1)),  # 2: front-right-top
            bm.verts.new((x0, y0, z1)),  # 3: front-left-top
            bm.verts.new((x0, y1, z0)),  # 4: back-left-bottom
            bm.verts.new((x1, y1, z0)),  # 5: back-right-bottom
            bm.verts.new((x1, y1, z1)),  # 6: back-right-top
            bm.verts.new((x0, y1, z1)),  # 7: back-left-top
        ]
        bm.faces.new([v[0], v[1], v[2], v[3]])  # front
        bm.faces.new([v[7], v[6], v[5], v[4]])  # back
        bm.faces.new([v[3], v[2], v[6], v[7]])  # top
        bm.faces.new([v[4], v[5], v[1], v[0]])  # bottom
        bm.faces.new([v[0], v[3], v[7], v[4]])  # left
        bm.faces.new([v[5], v[6], v[2], v[1]])  # right

    mesh = bpy.data.meshes.new("Steps")
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new("Steps", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = (-8, -3, 0)
    set_vertex_colors(obj, (0.3, 0.7, 0.4))
    return obj

def main():
    clear_scene()

    ramp = create_ramp()
    cylinder = create_cylinder()
    steps = create_steps()

    # Apply transforms so vertex positions are in world space before joining
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Join all objects into one mesh
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
