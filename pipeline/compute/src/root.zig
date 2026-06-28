// libmahlanya_compute — C ABI root
// Re-exports all public symbols from erosion.zig, dinf.zig, and voronoi.zig.
// `zig build` compiles this to libmahlanya_compute.so (Linux / macOS)
// or mahlanya_compute.dll (Windows).

pub usingnamespace @import("erosion.zig");
pub usingnamespace @import("dinf.zig");
pub usingnamespace @import("voronoi.zig");
