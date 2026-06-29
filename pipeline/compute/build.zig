const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{
        // Default to ReleaseFast for production builds;
        // CI uses Debug for test builds.
        .preferred_optimize_mode = .ReleaseFast,
    });

    // Shared library: libmahlanya_compute.so / .dylib / .dll
    const lib = b.addSharedLibrary(.{
        .name = "mahlanya_compute",
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });

    // SIMD target features (AVX2 on x86_64, NEON on ARM64)
    // The @Vector builtins auto-vectorise when the CPU feature is available.
    // Uncomment to force specific feature sets on dedicated build machines:
    // lib.root_module.cpu_features_add.addFeature(@intFromEnum(std.Target.x86.Feature.avx2));

    b.installArtifact(lib);

    // Static library target (for C++ FFI without dlopen)
    const static_lib = b.addStaticLibrary(.{
        .name = "mahlanya_compute_static",
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });
    b.installArtifact(static_lib);

    // Unit tests
    const unit_tests = b.addTest(.{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });
    const run_unit_tests = b.addRunArtifact(unit_tests);

    const test_step = b.step("test", "Run Zig unit tests for libmahlanya_compute");
    test_step.dependOn(&run_unit_tests.step);
}
