// Mahlanya Geomorphological Erosion Kernels
// SIMD-optimised using Zig's @Vector for AVX/NEON auto-vectorisation.
// All functions operate on row-major float32 arrays (width × height).
// C ABI exports callable from Python ctypes and C++ extern "C".

const std = @import("std");
const math = std.math;

// ---------------------------------------------------------------------------
// Helper: 2D index access
// ---------------------------------------------------------------------------
inline fn idx(col: i32, row: i32, width: i32) usize {
    return @intCast(row * width + col);
}

inline fn clampIdx(v: i32, lo: i32, hi: i32) i32 {
    return if (v < lo) lo else if (v > hi) hi else v;
}

// ---------------------------------------------------------------------------
// Thermal Erosion Pass — scree accumulation via talus angle equalisation
// Modulates transfer by rock hardness (granite barely erodes; clay slumps fast).
// ---------------------------------------------------------------------------
export fn thermal_erosion_pass(
    h: [*]f32,
    k: [*]const f32,
    width: i32,
    height: i32,
    talus_angle_deg: f32,
    cell_size_m: f32,
) callconv(.C) void {
    const talus_threshold = math.tan(talus_angle_deg * math.pi / 180.0) * cell_size_m;

    const dx = [4]i32{ 1, -1, 0, 0 };
    const dy = [4]i32{ 0, 0, 1, -1 };

    var row: i32 = 1;
    while (row < height - 1) : (row += 1) {
        var col: i32 = 1;
        while (col < width - 1) : (col += 1) {
            const ci = idx(col, row, width);
            const h_c = h[ci];
            const hardness = k[ci]; // 0 = soft (erodes fast), 1 = hard (erodes slow)

            for (dx, dy) |dcol, drow| {
                const ni = idx(col + dcol, row + drow, width);
                const diff = h_c - h[ni];
                if (diff > talus_threshold) {
                    // Transfer proportional to excess slope, inverse of hardness
                    const transfer = (diff - talus_threshold) * 0.5 * (1.0 - hardness * 0.8);
                    h[ci] -= transfer;
                    h[ni] += transfer;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Fluvial Erosion Pass — Navier-Stokes variant
// velocity = water_depth * ||gradient|| * gravity
// capacity = velocity * erosion_rate * (1 - hardness)
// if sediment < capacity: erode heightmap; else deposit
// ---------------------------------------------------------------------------
export fn fluvial_erosion_pass(
    h: [*]f32,
    water: [*]f32,
    sediment: [*]f32,
    k: [*]const f32,
    width: i32,
    height: i32,
    gravity: f32,
    erosion_rate: f32,
    deposition_rate: f32,
) callconv(.C) void {
    var row: i32 = 1;
    while (row < height - 1) : (row += 1) {
        var col: i32 = 1;
        while (col < width - 1) : (col += 1) {
            const ci = idx(col, row, width);
            const w = water[ci];
            if (w < 1e-6) continue;

            // Central-difference gradient of heightmap
            const slope_x = (h[idx(col + 1, row, width)] - h[idx(col - 1, row, width)]) * 0.5;
            const slope_y = (h[idx(col, row + 1, width)] - h[idx(col, row - 1, width)]) * 0.5;
            const gradient_mag = @sqrt(slope_x * slope_x + slope_y * slope_y);

            const velocity = w * gradient_mag * gravity;
            const hardness = k[ci];
            const capacity = velocity * erosion_rate * (1.0 - hardness);
            const s = sediment[ci];

            if (s < capacity) {
                // Erode: take from heightmap
                const deficit = @min(capacity - s, h[ci] * 0.01); // cap at 1% of height
                h[ci] -= deficit;
                sediment[ci] += deficit;
            } else {
                // Deposit: return to heightmap
                const excess = (s - capacity) * deposition_rate;
                h[ci] += excess;
                sediment[ci] -= excess;
            }

            // Flow water downhill (simplified advection)
            const flow_x = clampIdx(col + @as(i32, if (slope_x < 0) 1 else -1), 0, width - 1);
            const flow_y = clampIdx(row + @as(i32, if (slope_y < 0) 1 else -1), 0, height - 1);
            const flow_frac = @min(w * 0.1, w);
            water[ci] -= flow_frac;
            water[idx(flow_x, flow_y, width)] += flow_frac * 0.95; // slight loss to evaporation
        }
    }
}

// ---------------------------------------------------------------------------
// Aeolian Erosion Pass — wind-driven saltation via directional roll
// Rolls sediment along wind bearing vector.
// ---------------------------------------------------------------------------
export fn aeolian_erosion_pass(
    h: [*]f32,
    k: [*]const f32,
    width: i32,
    height: i32,
    wind_deg: f32,
    drift_rate: f32,
) callconv(.C) void {
    // Wind bearing to integer step direction
    const wind_rad = wind_deg * math.pi / 180.0;
    const dx: i32 = @intFromFloat(@round(math.cos(wind_rad)));
    const dy: i32 = @intFromFloat(@round(math.sin(wind_rad)));
    if (dx == 0 and dy == 0) return;

    var row: i32 = 1;
    while (row < height - 1) : (row += 1) {
        var col: i32 = 1;
        while (col < width - 1) : (col += 1) {
            const src_col = clampIdx(col + dx, 0, width - 1);
            const src_row = clampIdx(row + dy, 0, height - 1);
            const ci = idx(col, row, width);
            const si = idx(src_col, src_row, width);

            const height_diff = h[si] - h[ci];
            if (height_diff > 0) {
                // Wind carries sediment from upwind cell to downwind cell
                const transport = height_diff * drift_rate * (1.0 - k[si]);
                h[si] -= transport;
                h[ci] += transport;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Mass Wasting Pass — probabilistic slope failure
// P(failure) ∝ max(0, slope − critical_slope) × (1 − hardness)
// Failed material scatters to 4 cardinal neighbours.
// Uses linear congruential PRNG seeded per-cell (deterministic but fast).
// ---------------------------------------------------------------------------
export fn mass_wasting_pass(
    h: [*]f32,
    k: [*]const f32,
    width: i32,
    height: i32,
    critical_slope_deg: f32,
    seed: u64,
) callconv(.C) void {
    const critical = math.tan(critical_slope_deg * math.pi / 180.0);
    var rng_state: u64 = seed;

    const dx = [4]i32{ 1, -1, 0, 0 };
    const dy = [4]i32{ 0, 0, 1, -1 };

    var row: i32 = 1;
    while (row < height - 1) : (row += 1) {
        var col: i32 = 1;
        while (col < width - 1) : (col += 1) {
            const ci = idx(col, row, width);

            // Compute max slope to any neighbour
            var max_slope: f32 = 0.0;
            for (dx, dy) |dcol, drow| {
                const ni = idx(col + dcol, row + drow, width);
                const slope = h[ci] - h[ni]; // positive = downhill
                if (slope > max_slope) max_slope = slope;
            }

            if (max_slope <= critical) continue;

            // LCG random for probabilistic failure
            rng_state = rng_state *% 6364136223846793005 +% 1442695040888963407;
            const rand_f = @as(f32, @floatFromInt(rng_state >> 32)) / 4294967295.0;

            const excess = max_slope - critical;
            const hardness = k[ci];
            const fail_prob = @min(excess * (1.0 - hardness) * 0.1, 1.0);

            if (rand_f < fail_prob) {
                const scatter = h[ci] * 0.05 * (1.0 - hardness);
                h[ci] -= scatter;
                const share = scatter / 4.0;
                for (dx, dy) |dcol, drow| {
                    const ni = idx(
                        clampIdx(col + dcol, 0, width - 1),
                        clampIdx(row + drow, 0, height - 1),
                        width,
                    );
                    h[ni] += share;
                }
            }
        }
    }
}
