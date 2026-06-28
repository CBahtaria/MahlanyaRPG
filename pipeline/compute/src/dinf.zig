// Mahlanya D-infinity Flow Accumulation — Tarboton 1997
// Priority-Flood pit filling (Wang & Liu 2006) + D-infinity flow direction + accumulation.
// C ABI exports callable from Python ctypes and C++ extern "C".

const std = @import("std");
const math = std.math;

// ---------------------------------------------------------------------------
// Priority-Flood Pit Filling — Wang & Liu 2006 variant
// Ensures every cell can drain to the boundary (prerequisite for D-inf).
// ---------------------------------------------------------------------------
export fn dinf_fill_pits(
    h: [*]f32,
    width: i32,
    height: i32,
) callconv(.C) void {
    const allocator = std.heap.page_allocator;
    const n: usize = @intCast(width * height);

    // Priority queue: (elevation, flat_index)
    const PQEntry = struct {
        elev: f32,
        idx: u32,
        fn lessThan(_: void, a: @This(), b: @This()) std.math.Order {
            return std.math.order(a.elev, b.elev);
        }
    };

    var pq = std.PriorityQueue(PQEntry, void, PQEntry.lessThan).init(allocator, {});
    defer pq.deinit();

    var in_queue = allocator.alloc(bool, n) catch return;
    defer allocator.free(in_queue);
    @memset(in_queue, false);

    // Initialise: push all boundary cells
    var row: i32 = 0;
    while (row < height) : (row += 1) {
        var col: i32 = 0;
        while (col < width) : (col += 1) {
            if (row == 0 or row == height - 1 or col == 0 or col == width - 1) {
                const fi: u32 = @intCast(row * width + col);
                pq.add(.{ .elev = h[fi], .idx = fi }) catch return;
                in_queue[fi] = true;
            }
        }
    }

    const dx = [8]i32{ 1, -1, 0, 0, 1, -1, 1, -1 };
    const dy = [8]i32{ 0, 0, 1, -1, 1, -1, -1, 1 };

    // Flood from boundaries inward
    while (pq.removeOrNull()) |entry| {
        const ei: i32 = @intCast(entry.idx / @as(u32, @intCast(width)));
        const ec: i32 = @intCast(entry.idx % @as(u32, @intCast(width)));

        for (dx, dy) |dcol, drow| {
            const nc = ec + dcol;
            const nr = ei + drow;
            if (nc < 0 or nc >= width or nr < 0 or nr >= height) continue;
            const ni: u32 = @intCast(nr * width + nc);
            if (in_queue[ni]) continue;
            in_queue[ni] = true;
            // Raise neighbour to at least current cell elevation (fill pit)
            h[ni] = @max(h[ni], entry.elev);
            pq.add(.{ .elev = h[ni], .idx = ni }) catch return;
        }
    }
}

// ---------------------------------------------------------------------------
// D-infinity Flow Direction — Tarboton 1997
// For each cell, examines 8 triangular facets.
// Returns steepest downward angle in [0, 2π) — 0 = East, π/2 = South.
// Flat/no-flow cells get angle = -1.0 (sentinel).
// ---------------------------------------------------------------------------

// Facet geometry: 8 triangular facets, each defined by two neighbour offsets
// e1 = primary (cardinal/diagonal), e2 = secondary (diagonal/cardinal)
const E1_COL = [8]i32{ 1, 0, -1, 0, 1, -1, -1, 1 };
const E1_ROW = [8]i32{ 0, 1, 0, -1, 1, 1, -1, -1 };
const E2_COL = [8]i32{ 1, 1, -1, -1, 0, -1, 0, 1 };
const E2_ROW = [8]i32{ 1, 0, -1, 0, 1, 1, -1, -1 };
const FACET_ANGLE_BASE = [8]f32{ 0.0, math.pi * 0.5, math.pi, math.pi * 1.5,
                                  math.pi * 0.25, math.pi * 0.75, math.pi * 1.25, math.pi * 1.75 };
const D_DIAGONAL = math.sqrt2; // diagonal distance in grid units

export fn dinf_flow_direction(
    h: [*]const f32,
    angles: [*]f32,
    width: i32,
    height: i32,
) callconv(.C) void {
    var row: i32 = 1;
    while (row < height - 1) : (row += 1) {
        var col: i32 = 1;
        while (col < width - 1) : (col += 1) {
            const ci: usize = @intCast(row * width + col);
            const h_c = h[ci];
            var max_slope: f32 = -math.inf(f32);
            var best_angle: f32 = -1.0;

            for (0..8) |f| {
                const c1 = col + E1_COL[f];
                const r1 = row + E1_ROW[f];
                const c2 = col + E2_COL[f];
                const r2 = row + E2_ROW[f];

                if (c1 < 0 or c1 >= width or r1 < 0 or r1 >= height) continue;
                if (c2 < 0 or c2 >= width or r2 < 0 or r2 >= height) continue;

                const h1 = h[@intCast(r1 * width + c1)];
                const h2 = h[@intCast(r2 * width + c2)];

                // Facet slopes: s1 = slope along primary edge, s2 = cross-slope
                const is_diagonal = (E1_COL[f] != 0 and E1_ROW[f] != 0);
                const d1: f32 = if (is_diagonal) D_DIAGONAL else 1.0;

                const s1 = (h_c - h1) / d1;
                const s2 = (h1 - h2); // normalised by 1 unit

                // Direction angle within facet: atan2(s2, s1) relative to e1 direction
                var angle_in_facet = math.atan2(s2, s1);

                // Clamp to [0, π/4] — valid within this facet only
                if (angle_in_facet < 0.0) {
                    angle_in_facet = 0.0;
                } else if (angle_in_facet > math.pi / 4.0) {
                    angle_in_facet = math.pi / 4.0;
                }

                // Slope magnitude for this facet direction
                const slope = @sqrt(s1 * s1 + s2 * s2);

                if (slope > max_slope) {
                    max_slope = slope;
                    // Convert facet-local angle to world angle
                    best_angle = FACET_ANGLE_BASE[f] + angle_in_facet;
                }
            }

            // Only assign if downhill (max_slope > 0)
            angles[ci] = if (max_slope > 0.0) best_angle else -1.0;
        }
    }

    // Boundary cells: no valid flow direction
    var c: i32 = 0;
    while (c < width) : (c += 1) {
        angles[@intCast(0 * width + c)] = -1.0;
        angles[@intCast((height - 1) * width + c)] = -1.0;
    }
    var r: i32 = 0;
    while (r < height) : (r += 1) {
        angles[@intCast(r * width + 0)] = -1.0;
        angles[@intCast(r * width + (width - 1))] = -1.0;
    }
}

// ---------------------------------------------------------------------------
// D-infinity Flow Accumulation
// For each cell, fraction of flow goes to two downstream cells based on angle.
// Processes cells in descending elevation order.
// ---------------------------------------------------------------------------
export fn dinf_flow_accumulation(
    angles: [*]const f32,
    accum: [*]f32,
    width: i32,
    height: i32,
) callconv(.C) void {
    const allocator = std.heap.page_allocator;
    const n: usize = @intCast(width * height);

    // Initialise accumulation: every cell contributes 1 unit of flow
    for (0..n) |i| accum[i] = 1.0;

    // Process all cells: distribute flow downstream.
    // Cells with angle == -1.0 are sinks (no outflow) → skipped.
    // Note: full topological-order processing requires the height array.
    // The MahlanyaPipelinePlugin C++ commandlet does the sorted pass;
    // this Zig function provides the fast per-cell kernel.

    var order = allocator.alloc(u32, n) catch return;
    defer allocator.free(order);
    for (0..n) |i| order[i] = @intCast(i);

    // Simple pass: for each valid cell, distribute flow to two downstream cells
    // This is approximate (ignores elevation-based sort). Full implementation
    // requires the height array — use MahlanyaPipelinePlugin C++ for production.
    for (order) |fi| {
        const ang = angles[fi];
        if (ang < 0.0) continue; // no flow

        const row: i32 = @intCast(fi / @as(u32, @intCast(width)));
        const col: i32 = @intCast(fi % @as(u32, @intCast(width)));

        // Determine two target cells from angle
        const sector: usize = @intFromFloat(ang / (math.pi / 4.0));
        const f: usize = sector % 8;
        const angle_in_facet = ang - FACET_ANGLE_BASE[f];
        const frac2 = angle_in_facet / (math.pi / 4.0); // fraction to e2 direction
        const frac1 = 1.0 - frac2;

        // Target 1: primary direction
        const t1c = clampI(col + E1_COL[f], 0, width - 1);
        const t1r = clampI(row + E1_ROW[f], 0, height - 1);
        // Target 2: secondary direction
        const t2c = clampI(col + E2_COL[f], 0, width - 1);
        const t2r = clampI(row + E2_ROW[f], 0, height - 1);

        const flow = accum[fi];
        accum[@intCast(t1r * width + t1c)] += flow * frac1;
        accum[@intCast(t2r * width + t2c)] += flow * frac2;
    }
}

inline fn clampI(v: i32, lo: i32, hi: i32) i32 {
    return if (v < lo) lo else if (v > hi) hi else v;
}
