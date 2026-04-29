"""Generate the Phase 4 collision test course.
Output: resources/courses/phase4_collision.glb
Run: blender --background --python tools/models/phase4_collision.py

Blender is Z-up. The glTF exporter converts to Y-up for the runtime.
"""
import os
import bpy


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def material(name, color):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.0)
    return mat


def set_vertex_colors(obj, color):
    mesh = obj.data
    if not mesh.color_attributes:
        mesh.color_attributes.new(name="Color", type="FLOAT_COLOR", domain="CORNER")
    color_attr = mesh.color_attributes[0]
    for item in color_attr.data:
        item.color = (*color, 1.0)


def create_mesh(name, verts, faces, mat, color):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(mat)
    set_vertex_colors(obj, color)
    return obj


def quad(name, x0, y0, z0, x1, y1, z1, mat, color):
    # Vertices are ordered for an upward normal when the quad is horizontal.
    return create_mesh(name, [(x0, y0, z0), (x1, y0, z0), (x1, y1, z1), (x0, y1, z1)], [(0, 1, 2, 3)], mat, color)


def box(name, x0, y0, z0, x1, y1, z1, mat, color):
    verts = [
        (x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
        (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1),
    ]
    faces = [
        (0, 1, 2, 3),  # bottom
        (4, 7, 6, 5),  # top
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (3, 7, 4, 0),
    ]
    return create_mesh(name, verts, faces, mat, color)


def main():
    clear_scene()

    normal = material("normal_collision", (0.35, 0.72, 0.35))
    steep = material("steep_collision", (0.95, 0.72, 0.22))
    wall = material("wall_collision", (0.9, 0.22, 0.18))
    ice = material("ice_collision", (0.25, 0.8, 1.0))
    ceiling = material("ceiling_collision", (0.65, 0.45, 0.9))

    # Main floor with a clear drop edge at positive Y.
    quad("flat_ground", -18, -12, 0, 18, 8, 0, normal, (0.35, 0.72, 0.35))
    quad("lower_drop_floor", -8, 9, -2.5, 8, 16, -2.5, normal, (0.22, 0.5, 0.25))

    # Walkable ramp: about 25 degrees.
    quad("walkable_slope_25", -16, -8, 0, -10, -2, 2.8, normal, (0.45, 0.78, 0.35))

    # Steep ramp: about 40 degrees.
    quad("steep_slope_40", -8, -8, 0, -2, -2, 5.0, steep, (0.95, 0.72, 0.22))

    # Wall-like slope: about 65 degrees.
    quad("wall_slope_65", 1, -8, 0, 4, -5, 6.4, wall, (0.9, 0.22, 0.18))

    # Low and high step ledges.
    box("step_small_025", 6, -8, 0, 10, -4, 0.25, normal, (0.45, 0.7, 0.45))
    box("step_tall_045", 11, -8, 0, 15, -4, 0.45, wall, (0.8, 0.35, 0.25))

    # Wall slide test and ceiling bonk slab.
    box("vertical_wall", 14, -2, 0, 14.3, 5, 3.0, wall, (0.9, 0.22, 0.18))
    box("ceiling_slab", 6, 0, 2.4, 12, 5, 2.75, ceiling, (0.65, 0.45, 0.9))

    # Ice patch on flat ground.
    quad("ice_patch", -6, 3, 0.02, 4, 7, 0.02, ice, (0.25, 0.8, 1.0))

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, "..", ".."))
    out_path = os.path.join(project_root, "resources", "courses", "phase4_collision.glb")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    bpy.ops.export_scene.gltf(filepath=out_path, export_format="GLB")
    print(f"Exported: {out_path}")


if __name__ == "__main__":
    main()
