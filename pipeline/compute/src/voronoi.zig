// Mahlanya Voronoi & Lloyd Relaxation
// Centroidal Voronoi optimisation for Umuti settlement layout.
// Taboo arc exclusion enforced during relaxation (240°–300° = luhlanga zone).
// C ABI exports callable from Python ctypes and C++ extern "C".

const std = @import("std");
const math = std.math;

// ---------------------------------------------------------------------------
// Lloyd Relaxation with Taboo Arc
//
// pts: flat array of [x0, y0, x1, y1, ...] in normalised [0,1] space
// n: number of points
// iters: number of Lloyd iterations
// taboo_start/end: bearing degrees [0,360) of forbidden arc (e.g. 240,300)
//
// After each iteration, any point that lands inside the taboo arc is
// reflected/bumped to the nearest valid bearing just outside the arc.
// ---------------------------------------------------------------------------
export fn lloyd_relax(
    pts: [*]f64,
    n: i32,
    iters: i32,
    taboo_start: f64,
    taboo_end: f64,
) callconv(.C) void {
    const allocator = std.heap.page_allocator;
    const np: usize = @intCast(n);

    // Voronoi weights: each point accumulates a centroid sum
    var centroid_x = allocator.alloc(f64, np) catch return;
    var centroid_y = allocator.alloc(f64, np) catch return;
    var count = allocator.alloc(u32, np) catch return;
    defer allocator.free(centroid_x);
    defer allocator.free(centroid_y);
    defer allocator.free(count);

    const SAMPLE_GRID = 200; // resolution of virtual sample grid
    const GRID_F: f64 = @floatFromInt(SAMPLE_GRID);

    var iter: i32 = 0;
    while (iter < iters) : (iter += 1) {
        @memset(centroid_x, 0.0);
        @memset(centroid_y, 0.0);
        @memset(count, 0);

        // Sample grid: assign each grid cell to nearest point → accumulate centroid
        var gy: u32 = 0;
        while (gy < SAMPLE_GRID) : (gy += 1) {
            const sy: f64 = (@as(f64, @floatFromInt(gy)) + 0.5) / GRID_F;
            var gx: u32 = 0;
            while (gx < SAMPLE_GRID) : (gx += 1) {
                const sx: f64 = (@as(f64, @floatFromInt(gx)) + 0.5) / GRID_F;

                // Find nearest Voronoi site
                var best_dist: f64 = math.inf(f64);
                var best_i: usize = 0;
                for (0..np) |i| {
                    const px = pts[i * 2];
                    const py = pts[i * 2 + 1];
                    const dx = sx - px;
                    const dy = sy - py;
                    const d2 = dx * dx + dy * dy;
                    if (d2 < best_dist) {
                        best_dist = d2;
                        best_i = i;
                    }
                }
                centroid_x[best_i] += sx;
                centroid_y[best_i] += sy;
                count[best_i] += 1;
            }
        }

        // Move each point to centroid of its Voronoi cell
        for (0..np) |i| {
            if (count[i] == 0) continue;
            const cnt: f64 = @floatFromInt(count[i]);
            var nx = centroid_x[i] / cnt;
            var ny = centroid_y[i] / cnt;

            // Enforce taboo arc: convert point to bearing (centre = 0.5, 0.5)
            const bx = nx - 0.5;
            const by = ny - 0.5;
            if (@abs(bx) > 1e-9 or @abs(by) > 1e-9) {
                // Bearing in degrees: 0 = North (+Y), clockwise
                var bearing = math.atan2(bx, by) * 180.0 / math.pi;
                if (bearing < 0.0) bearing += 360.0;

                if (bearing >= taboo_start and bearing <= taboo_end) {
                    // Bump to nearest edge of taboo arc
                    const dist_to_start = bearing - taboo_start;
                    const dist_to_end = taboo_end - bearing;
                    const bump_bearing = if (dist_to_start < dist_to_end)
                        taboo_start - 2.0
                    else
                        taboo_end + 2.0;

                    // Reconstruct point at same radius, bumped bearing
                    const radius = @sqrt(bx * bx + by * by);
                    const bump_rad = bump_bearing * math.pi / 180.0;
                    nx = 0.5 + radius * math.sin(bump_rad);
                    ny = 0.5 + radius * math.cos(bump_rad);
                }
            }

            // Clamp to valid [0,1] domain
            pts[i * 2] = math.clamp(nx, 0.0, 1.0);
            pts[i * 2 + 1] = math.clamp(ny, 0.0, 1.0);
        }
    }
}

// ---------------------------------------------------------------------------
// Compute Voronoi Cell Areas (normalised)
// Returns area[i] = fraction of [0,1]² owned by site i.
// Used for social importance weight derivation (larger cell = higher status).
// ---------------------------------------------------------------------------
export fn voronoi_cell_areas(
    pts: [*]const f64,
    n: i32,
    areas: [*]f64,
) callconv(.C) void {
    const np: usize = @intCast(n);
    for (0..np) |i| areas[i] = 0.0;

    const SAMPLE_GRID = 200;
    const GRID_F: f64 = @floatFromInt(SAMPLE_GRID);
    const CELL_AREA: f64 = 1.0 / (GRID_F * GRID_F);

    var gy: u32 = 0;
    while (gy < SAMPLE_GRID) : (gy += 1) {
        const sy: f64 = (@as(f64, @floatFromInt(gy)) + 0.5) / GRID_F;
        var gx: u32 = 0;
        while (gx < SAMPLE_GRID) : (gx += 1) {
            const sx: f64 = (@as(f64, @floatFromInt(gx)) + 0.5) / GRID_F;
            var best_dist: f64 = math.inf(f64);
            var best_i: usize = 0;
            for (0..np) |i| {
                const dx = sx - pts[i * 2];
                const dy = sy - pts[i * 2 + 1];
                const d2 = dx * dx + dy * dy;
                if (d2 < best_dist) {
                    best_dist = d2;
                    best_i = i;
                }
            }
            areas[best_i] += CELL_AREA;
        }
    }
}
