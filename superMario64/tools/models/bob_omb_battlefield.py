"""Generate a simplified Bob-omb Battlefield course.
Output: resources/courses/bob_omb_battlefield.glb
Run: blender --background --python tools/models/bob_omb_battlefield.py

Blender is Z-up. The glTF exporter converts to Y-up for the runtime.
"""
import math
import os
import bpy
import bmesh


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
    for item in mesh.color_attributes[0].data:
        item.color = (*color, 1.0)


def set_vertex_colors_bmesh(bm, color_layer, color):
    """Set vertex colors on all faces in a bmesh via a color layer."""
    for face in bm.faces:
        for loop in face.loops:
            loop[color_layer] = (*color, 1.0)


def create_mesh(name, verts, faces, mat, color):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(mat)
    set_vertex_colors(obj, color)
    return obj


def box(name, x0, y0, z0, x1, y1, z1, mat, color):
    verts = [
        (x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
        (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1),
    ]
    faces = [
        (0, 1, 2, 3), (4, 7, 6, 5),
        (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0),
    ]
    return create_mesh(name, verts, faces, mat, color)


def quad(name, x0, y0, z0, x1, y1, z1, mat, color):
    return create_mesh(
        name,
        [(x0, y0, z0), (x1, y0, z0), (x1, y1, z1), (x0, y1, z1)],
        [(0, 1, 2, 3)],
        mat, color,
    )


# ---------------------------------------------------------------------------
# Flat-area definitions (Blender X/Y coords).  Inside these rectangles the
# ground height is clamped to 0 so Mario has flat terrain to stand on.
# Each entry is (x_min, x_max, y_min, y_max).
# ---------------------------------------------------------------------------
FLAT_ZONES = [
    (-12, 12, -45, -25),   # start area
    (-12, 12, -30, -10),   # meadow / goomba area
    (17, 33, -13, 3),      # chain chomp enclosure
    (-25, -15, -20, -10),  # cannon pit area
]


def ground_height(x, y):
    """Sine-based rolling hills, flattened in specific gameplay zones."""
    for (xmin, xmax, ymin, ymax) in FLAT_ZONES:
        if xmin <= x <= xmax and ymin <= y <= ymax:
            return 0.0
    return 1.5 * math.sin(x / 10.0) * math.cos(y / 12.0)


def create_ground_plane(mat, color):
    """80x80 subdivided grid with sine-wave height displacement."""
    extent = 40  # half-size, so -40..40
    spacing = 2  # ~2 units per vertex
    steps = int(2 * extent / spacing)  # number of subdivisions per axis

    bm = bmesh.new()
    color_layer = bm.loops.layers.color.new("Color")

    # Create vertex grid.
    grid = {}
    for iy in range(steps + 1):
        for ix in range(steps + 1):
            x = -extent + ix * spacing
            y = -extent + iy * spacing
            z = ground_height(x, y)
            grid[(ix, iy)] = bm.verts.new((x, y, z))

    bm.verts.ensure_lookup_table()

    # Create quad faces.
    for iy in range(steps):
        for ix in range(steps):
            v0 = grid[(ix, iy)]
            v1 = grid[(ix + 1, iy)]
            v2 = grid[(ix + 1, iy + 1)]
            v3 = grid[(ix, iy + 1)]
            face = bm.faces.new([v0, v1, v2, v3])
            for loop in face.loops:
                loop[color_layer] = (*color, 1.0)

    mesh = bpy.data.meshes.new("Ground")
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new("Ground", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(mat)
    return obj


def create_mountain(mat_stone, color_stone, mat_path, color_path):
    """Truncated cone mountain with a spiral ramp path."""
    objects = []
    center_x, center_y = 0, 20
    base_r = 15.0
    top_r = 5.0
    height = 20.0
    segments = 32

    # --- Main cone body ---
    bm = bmesh.new()
    color_layer = bm.loops.layers.color.new("Color")

    bottom_verts = []
    top_verts = []
    for i in range(segments):
        angle = 2.0 * math.pi * i / segments
        bx = center_x + base_r * math.cos(angle)
        by = center_y + base_r * math.sin(angle)
        tx = center_x + top_r * math.cos(angle)
        ty = center_y + top_r * math.sin(angle)
        bottom_verts.append(bm.verts.new((bx, by, 0)))
        top_verts.append(bm.verts.new((tx, ty, height)))

    bm.verts.ensure_lookup_table()

    # Side faces.
    for i in range(segments):
        j = (i + 1) % segments
        face = bm.faces.new([bottom_verts[i], bottom_verts[j],
                             top_verts[j], top_verts[i]])
        for loop in face.loops:
            loop[color_layer] = (*color_stone, 1.0)

    # Top cap (summit platform).
    summit_color = (0.4, 0.4, 0.36)
    top_face = bm.faces.new(list(reversed(top_verts)))
    for loop in top_face.loops:
        loop[color_layer] = (*summit_color, 1.0)

    # Bottom cap.
    bot_face = bm.faces.new(bottom_verts)
    for loop in bot_face.loops:
        loop[color_layer] = (*color_stone, 1.0)

    mesh = bpy.data.meshes.new("Mountain")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("Mountain", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(mat_stone)
    objects.append(obj)

    # --- Spiral ramp ---
    # The ramp spirals 1.5 turns around the mountain from base to summit.
    ramp_width = 3.0
    ramp_turns = 1.5
    ramp_segments = int(segments * ramp_turns)
    ramp_verts_inner = []
    ramp_verts_outer = []

    bm2 = bmesh.new()
    color_layer2 = bm2.loops.layers.color.new("Color")

    for i in range(ramp_segments + 1):
        t = i / ramp_segments  # 0..1
        angle = 2.0 * math.pi * ramp_turns * t
        z = t * height
        # Radius at this height (interpolate between base and top).
        r = base_r + t * (top_r - base_r)
        # Inner edge sits on the cone surface, outer edge extends outward.
        inner_r = r
        outer_r = r + ramp_width
        ix = center_x + inner_r * math.cos(angle)
        iy_ = center_y + inner_r * math.sin(angle)
        ox = center_x + outer_r * math.cos(angle)
        oy = center_y + outer_r * math.sin(angle)
        ramp_verts_inner.append(bm2.verts.new((ix, iy_, z)))
        ramp_verts_outer.append(bm2.verts.new((ox, oy, z)))

    bm2.verts.ensure_lookup_table()

    for i in range(ramp_segments):
        face = bm2.faces.new([
            ramp_verts_inner[i], ramp_verts_outer[i],
            ramp_verts_outer[i + 1], ramp_verts_inner[i + 1],
        ])
        for loop in face.loops:
            loop[color_layer2] = (*color_path, 1.0)

    mesh2 = bpy.data.meshes.new("SpiralRamp")
    bm2.to_mesh(mesh2)
    bm2.free()
    obj2 = bpy.data.objects.new("SpiralRamp", mesh2)
    bpy.context.scene.collection.objects.link(obj2)
    obj2.data.materials.append(mat_path)
    objects.append(obj2)

    return objects


def create_bridge(mat, color):
    """Narrow wooden bridge spanning a ravine."""
    bx, by, bz = -15, 10, 3
    length = 12
    width = 2
    thickness = 0.3
    objects = []

    # Bridge deck.
    obj = box("Bridge_deck", bx - width / 2, by, bz,
              bx + width / 2, by + length, bz + thickness,
              mat, color)
    objects.append(obj)

    # Ravine walls: two walls on each side of the bridge creating the gap.
    ravine_depth = 4
    ravine_color = (0.45, 0.4, 0.3)
    wall_len = length + 2
    # Left wall.
    objects.append(box("Ravine_wall_L", bx - width / 2 - 1, by - 1, bz - ravine_depth,
                       bx - width / 2, by + wall_len - 1, bz + thickness,
                       mat, ravine_color))
    # Right wall.
    objects.append(box("Ravine_wall_R", bx + width / 2, by - 1, bz - ravine_depth,
                       bx + width / 2 + 1, by + wall_len - 1, bz + thickness,
                       mat, ravine_color))
    # Ravine floor.
    objects.append(box("Ravine_floor", bx - width / 2 - 1, by - 1, bz - ravine_depth,
                       bx + width / 2 + 1, by + wall_len - 1, bz - ravine_depth + 0.3,
                       mat, ravine_color))

    # Approach ramps on each side so you can walk up to the bridge.
    ramp_color = (0.4, 0.55, 0.2)
    # South approach ramp (from ground z=0 up to bridge z=3).
    ramp_len = 6
    ramp_verts = [
        (bx - width / 2, by - ramp_len, 0),
        (bx + width / 2, by - ramp_len, 0),
        (bx + width / 2, by, bz),
        (bx - width / 2, by, bz),
    ]
    objects.append(create_mesh("Bridge_ramp_S", ramp_verts, [(0, 1, 2, 3)], mat, ramp_color))
    # North approach ramp.
    ramp_verts_n = [
        (bx - width / 2, by + length, bz),
        (bx + width / 2, by + length, bz),
        (bx + width / 2, by + length + ramp_len, 0),
        (bx - width / 2, by + length + ramp_len, 0),
    ]
    objects.append(create_mesh("Bridge_ramp_N", ramp_verts_n, [(0, 1, 2, 3)], mat, ramp_color))

    return objects


def create_floating_island(mat_grass, color_grass, mat_dirt, color_dirt):
    """Small floating island in the sky."""
    cx, cy, cz = 20, 15, 15
    half = 3.0
    thickness = 2.0
    objects = []

    # Top surface (grass).
    objects.append(quad("FloatIsland_top",
                        cx - half, cy - half, cz,
                        cx + half, cy + half, cz,
                        mat_grass, color_grass))
    # Bottom.
    objects.append(quad("FloatIsland_bottom",
                        cx + half, cy - half, cz - thickness,
                        cx - half, cy + half, cz - thickness,
                        mat_dirt, color_dirt))
    # Four sides (dirt).
    # Front (y = cy - half).
    objects.append(create_mesh("FloatIsland_front",
        [(cx - half, cy - half, cz - thickness),
         (cx + half, cy - half, cz - thickness),
         (cx + half, cy - half, cz),
         (cx - half, cy - half, cz)],
        [(0, 1, 2, 3)], mat_dirt, color_dirt))
    # Back (y = cy + half).
    objects.append(create_mesh("FloatIsland_back",
        [(cx + half, cy + half, cz - thickness),
         (cx - half, cy + half, cz - thickness),
         (cx - half, cy + half, cz),
         (cx + half, cy + half, cz)],
        [(0, 1, 2, 3)], mat_dirt, color_dirt))
    # Left (x = cx - half).
    objects.append(create_mesh("FloatIsland_left",
        [(cx - half, cy + half, cz - thickness),
         (cx - half, cy - half, cz - thickness),
         (cx - half, cy - half, cz),
         (cx - half, cy + half, cz)],
        [(0, 1, 2, 3)], mat_dirt, color_dirt))
    # Right (x = cx + half).
    objects.append(create_mesh("FloatIsland_right",
        [(cx + half, cy - half, cz - thickness),
         (cx + half, cy + half, cz - thickness),
         (cx + half, cy + half, cz),
         (cx + half, cy - half, cz)],
        [(0, 1, 2, 3)], mat_dirt, color_dirt))

    return objects


def create_chain_chomp_area(mat_fence, color_fence, mat_ground, color_ground):
    """Fenced enclosure for the chain chomp."""
    cx, cy = 25, -5
    half = 4.0
    post_size = 0.3
    post_height = 1.5
    rail_height = 0.8
    rail_thickness = 0.15
    objects = []

    # Ground pad (slightly raised so it's visible).
    objects.append(quad("ChainChomp_ground",
                        cx - half, cy - half, 0.02,
                        cx + half, cy + half, 0.02,
                        mat_ground, color_ground))

    # Fence posts at corners and midpoints.
    post_positions = [
        (cx - half, cy - half), (cx, cy - half), (cx + half, cy - half),
        (cx + half, cy - half), (cx + half, cy), (cx + half, cy + half),
        (cx + half, cy + half), (cx, cy + half), (cx - half, cy + half),
        (cx - half, cy + half), (cx - half, cy), (cx - half, cy - half),
    ]
    # Deduplicate corners.
    seen = set()
    unique_posts = []
    for p in post_positions:
        key = (round(p[0], 2), round(p[1], 2))
        if key not in seen:
            seen.add(key)
            unique_posts.append(p)

    for i, (px, py) in enumerate(unique_posts):
        objects.append(box(f"FencePost_{i}",
                          px - post_size, py - post_size, 0,
                          px + post_size, py + post_size, post_height,
                          mat_fence, color_fence))

    # Horizontal rails along each side.
    # South rail (y = cy - half).
    objects.append(box("FenceRail_S",
                       cx - half, cy - half - rail_thickness, rail_height - rail_thickness,
                       cx + half, cy - half + rail_thickness, rail_height + rail_thickness,
                       mat_fence, color_fence))
    # North rail.
    objects.append(box("FenceRail_N",
                       cx - half, cy + half - rail_thickness, rail_height - rail_thickness,
                       cx + half, cy + half + rail_thickness, rail_height + rail_thickness,
                       mat_fence, color_fence))
    # East rail.
    objects.append(box("FenceRail_E",
                       cx + half - rail_thickness, cy - half, rail_height - rail_thickness,
                       cx + half + rail_thickness, cy + half, rail_height + rail_thickness,
                       mat_fence, color_fence))
    # West rail (with opening for gate — only half length).
    objects.append(box("FenceRail_W_top",
                       cx - half - rail_thickness, cy, rail_height - rail_thickness,
                       cx - half + rail_thickness, cy + half, rail_height + rail_thickness,
                       mat_fence, color_fence))
    objects.append(box("FenceRail_W_bot",
                       cx - half - rail_thickness, cy - half, rail_height - rail_thickness,
                       cx - half + rail_thickness, cy - 1.0, rail_height + rail_thickness,
                       mat_fence, color_fence))

    return objects


def create_cannon_pit(mat, color):
    """Small depression in the ground for the cannon."""
    cx, cy = -20, -15
    radius = 1.0
    depth = 1.0
    segments = 12
    objects = []

    bm = bmesh.new()
    color_layer = bm.loops.layers.color.new("Color")

    # Center vertex at the bottom.
    center_bottom = bm.verts.new((cx, cy, -depth))
    ring_bottom = []
    ring_top = []
    for i in range(segments):
        angle = 2.0 * math.pi * i / segments
        rx = cx + radius * math.cos(angle)
        ry = cy + radius * math.sin(angle)
        ring_bottom.append(bm.verts.new((rx, ry, -depth)))
        ring_top.append(bm.verts.new((rx, ry, 0)))

    bm.verts.ensure_lookup_table()

    # Bottom fan.
    for i in range(segments):
        j = (i + 1) % segments
        face = bm.faces.new([center_bottom, ring_bottom[i], ring_bottom[j]])
        for loop in face.loops:
            loop[color_layer] = (*color, 1.0)

    # Walls.
    for i in range(segments):
        j = (i + 1) % segments
        face = bm.faces.new([ring_bottom[i], ring_top[i], ring_top[j], ring_bottom[j]])
        for loop in face.loops:
            loop[color_layer] = (*color, 1.0)

    mesh = bpy.data.meshes.new("CannonPit")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("CannonPit", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(mat)
    objects.append(obj)
    return objects


def create_gate(mat, color):
    """Stone gate with an archway near the chain chomp area."""
    gx, gy = 25, -15
    wall_w = 4.0
    wall_h = 3.0
    wall_d = 0.5
    arch_w = 2.0
    objects = []

    # Left pillar.
    objects.append(box("Gate_pillar_L",
                       gx - wall_w / 2, gy - wall_d / 2, 0,
                       gx - arch_w / 2, gy + wall_d / 2, wall_h,
                       mat, color))
    # Right pillar.
    objects.append(box("Gate_pillar_R",
                       gx + arch_w / 2, gy - wall_d / 2, 0,
                       gx + wall_w / 2, gy + wall_d / 2, wall_h,
                       mat, color))
    # Top beam (archway lintel).
    objects.append(box("Gate_lintel",
                       gx - wall_w / 2, gy - wall_d / 2, wall_h * 0.7,
                       gx + wall_w / 2, gy + wall_d / 2, wall_h,
                       mat, color))

    return objects


def create_start_platform(mat, color):
    """Small stone platform at the spawn point."""
    cx, cy = 0, -35
    half = 3.0
    height = 0.4
    objects = []
    objects.append(box("StartPlatform",
                       cx - half, cy - half, 0,
                       cx + half, cy + half, height,
                       mat, color))
    return objects


def create_meadow_patch(mat, color):
    """Lighter green meadow area to the south."""
    return [quad("Meadow",
                 -12, -30, 0.03,
                 12, -10, 0.03,
                 mat, color)]


def main():
    clear_scene()

    # ----- Materials -----
    mat_grass = material("grass", (0.35, 0.6, 0.2))
    mat_stone = material("stone", (0.5, 0.5, 0.45))
    mat_path = material("dirt_path", (0.5, 0.35, 0.15))
    mat_wood = material("wood", (0.6, 0.4, 0.2))
    mat_dirt = material("dirt", (0.45, 0.3, 0.12))
    mat_dark_stone = material("dark_stone", (0.35, 0.35, 0.3))
    mat_fence = material("fence", (0.55, 0.35, 0.15))
    mat_meadow = material("meadow", (0.45, 0.7, 0.25))

    # ----- Colors -----
    color_grass = (0.35, 0.6, 0.2)
    color_stone = (0.5, 0.5, 0.45)
    color_path = (0.5, 0.35, 0.15)
    color_wood = (0.6, 0.4, 0.2)
    color_grass_top = (0.4, 0.65, 0.25)
    color_dirt = (0.45, 0.3, 0.12)
    color_dark_stone = (0.35, 0.35, 0.3)
    color_fence = (0.55, 0.35, 0.15)
    color_meadow = (0.45, 0.7, 0.25)

    all_objects = []

    # 1. Main ground plane with rolling hills.
    ground = create_ground_plane(mat_grass, color_grass)
    all_objects.append(ground)

    # 2 & 3. Central mountain with spiral ramp and summit platform.
    mountain_objs = create_mountain(mat_stone, color_stone, mat_path, color_path)
    all_objects.extend(mountain_objs)

    # 4. Meadow area (lighter green overlay).
    meadow_objs = create_meadow_patch(mat_meadow, color_meadow)
    all_objects.extend(meadow_objs)

    # 5. Bridge spanning a ravine.
    bridge_objs = create_bridge(mat_wood, color_wood)
    all_objects.extend(bridge_objs)

    # 6. Floating island.
    island_objs = create_floating_island(mat_grass, color_grass_top, mat_dirt, color_dirt)
    all_objects.extend(island_objs)

    # 7. Chain Chomp fenced area.
    chomp_objs = create_chain_chomp_area(mat_fence, color_fence, mat_grass, color_grass)
    all_objects.extend(chomp_objs)

    # 8. Cannon pit.
    cannon_objs = create_cannon_pit(mat_dark_stone, color_dark_stone)
    all_objects.extend(cannon_objs)

    # 9. Stone gate.
    gate_objs = create_gate(mat_stone, color_stone)
    all_objects.extend(gate_objs)

    # 10. Start area platform.
    start_objs = create_start_platform(mat_stone, color_stone)
    all_objects.extend(start_objs)

    # Apply all transforms.
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Join all objects into one mesh.
    bpy.context.view_layer.objects.active = all_objects[0]
    bpy.ops.object.join()

    # Triangulate for better collision detection.
    obj = bpy.context.active_object
    mod = obj.modifiers.new(name="Triangulate", type='TRIANGULATE')
    mod.quad_method = 'BEAUTY'
    mod.ngon_method = 'BEAUTY'
    bpy.ops.object.modifier_apply(modifier=mod.name)

    # Export.
    bpy.ops.object.select_all(action="SELECT")
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, "..", ".."))
    out_path = os.path.join(project_root, "resources", "courses", "bob_omb_battlefield.glb")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format="GLB",
        export_normals=True,
        use_selection=True,
    )
    print(f"Exported: {out_path}")


if __name__ == "__main__":
    main()
