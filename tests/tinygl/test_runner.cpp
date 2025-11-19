// Copyright (c) 2025 HelixScreen Project
// SPDX-License-Identifier: GPL-3.0-or-later
//
// TinyGL Test Runner - Main test execution program

#include "tinygl_test_framework.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace tinygl_test;

// ============================================================================
// Test Verification Infrastructure
// ============================================================================

struct TestResult {
    std::string test_name;
    bool passed;
    std::string failure_reason;
    ImageMetrics metrics;
};

std::vector<TestResult> g_test_results;
bool g_verify_mode = false; // Global flag for verification mode

// Verification thresholds
constexpr double MIN_PSNR = 30.0;       // Peak Signal-to-Noise Ratio (dB)
constexpr double MIN_SSIM = 0.95;       // Structural Similarity Index
constexpr double MAX_PIXEL_DIFF = 10.0; // Maximum single pixel difference

bool verify_rendering(TinyGLTestFramework& framework, const std::string& test_name,
                      const std::string& reference_filename) {
    // Capture current rendered framebuffer
    auto rendered = framework.capture_framebuffer_rgb();

    // Load reference image
    int ref_width, ref_height;
    auto reference = TinyGLTestFramework::load_ppm(reference_filename, ref_width, ref_height);

    if (reference.empty()) {
        std::cout << "\n  ❌ " << test_name << ": Reference image not found: " << reference_filename
                  << "\n";
        std::cout << "     Run 'make test-tinygl-reference' to generate reference images.\n";

        g_test_results.push_back(
            {test_name, false, "Reference image missing: " + reference_filename, {}});
        return false;
    }

    // Verify dimensions match
    int width = 800, height = 600; // Framework default size
    if (ref_width != width || ref_height != height) {
        std::cout << "\n  ❌ " << test_name << ": Dimension mismatch\n";
        std::cout << "     Expected: " << width << "×" << height << "\n";
        std::cout << "     Reference: " << ref_width << "×" << ref_height << "\n";

        g_test_results.push_back({test_name, false, "Dimension mismatch", {}});
        return false;
    }

    // Compare images
    auto metrics = TinyGLTestFramework::compare_images(reference, rendered, width, height);

    // Check thresholds
    bool passed = (metrics.psnr >= MIN_PSNR) && (metrics.ssim >= MIN_SSIM) &&
                  (metrics.max_diff <= MAX_PIXEL_DIFF);

    if (passed) {
        std::cout << "\n  ✅ " << test_name << ": PASSED\n";
        std::cout << "     PSNR: " << std::fixed << std::setprecision(2) << metrics.psnr << " dB"
                  << " (threshold: " << MIN_PSNR << " dB)\n";
        std::cout << "     SSIM: " << std::fixed << std::setprecision(4) << metrics.ssim
                  << " (threshold: " << MIN_SSIM << ")\n";
    } else {
        std::cout << "\n  ❌ " << test_name << ": FAILED\n";
        std::cout << "     PSNR: " << std::fixed << std::setprecision(2) << metrics.psnr << " dB"
                  << " (threshold: " << MIN_PSNR << " dB) "
                  << (metrics.psnr < MIN_PSNR ? "❌" : "✓") << "\n";
        std::cout << "     SSIM: " << std::fixed << std::setprecision(4) << metrics.ssim
                  << " (threshold: " << MIN_SSIM << ") " << (metrics.ssim < MIN_SSIM ? "❌" : "✓")
                  << "\n";
        std::cout << "     Max pixel diff: " << static_cast<int>(metrics.max_diff) << "/255"
                  << " (threshold: " << static_cast<int>(MAX_PIXEL_DIFF) << ") "
                  << (metrics.max_diff > MAX_PIXEL_DIFF ? "❌" : "✓") << "\n";
        std::cout << "     Different pixels: " << metrics.diff_pixels << "\n";

        // Save diff image for debugging
        std::string diff_filename = "tests/tinygl/output/FAILED_" + test_name + "_diff.ppm";
        auto diff_img = utils::create_diff_image(reference, rendered, width, height, 10.0f);

        std::ofstream diff_file(diff_filename, std::ios::binary);
        if (diff_file.is_open()) {
            diff_file << "P6\n" << width << " " << height << "\n255\n";
            diff_file.write(reinterpret_cast<const char*>(diff_img.data()), diff_img.size());
            std::cout << "     Diff image saved: " << diff_filename << "\n";
        }
    }

    g_test_results.push_back({test_name, passed, passed ? "" : "Metrics below threshold", metrics});

    return passed;
}

void print_separator(const std::string& title = "") {
    if (title.empty()) {
        std::cout << "═══════════════════════════════════════════════════════════════\n";
    } else {
        int padding = (60 - title.length()) / 2;
        std::cout << "═";
        for (int i = 0; i < padding; i++)
            std::cout << "═";
        std::cout << " " << title << " ";
        for (int i = 0; i < padding; i++)
            std::cout << "═";
        std::cout << "═\n";
    }
}

void print_metrics(const std::string& name, const ImageMetrics& metrics) {
    std::cout << "\n📊 " << name << " Image Quality Metrics:\n";
    std::cout << "  • MSE:          " << std::fixed << std::setprecision(2) << metrics.mse << "\n";
    std::cout << "  • PSNR:         " << std::fixed << std::setprecision(2) << metrics.psnr
              << " dB\n";
    std::cout << "  • SSIM:         " << std::fixed << std::setprecision(4) << metrics.ssim << "\n";
    std::cout << "  • Max Diff:     " << static_cast<int>(metrics.max_diff) << "/255\n";
    std::cout << "  • Diff Pixels:  " << metrics.diff_pixels << "\n";
}

void print_perf(const std::string& name, const PerfMetrics& metrics) {
    std::cout << "\n⚡ " << name << " Performance Metrics:\n";
    std::cout << "  • Frame Time:      " << std::fixed << std::setprecision(2)
              << metrics.frame_time_ms << " ms\n";
    std::cout << "  • FPS:             " << std::fixed << std::setprecision(1)
              << (1000.0 / metrics.frame_time_ms) << "\n";
    std::cout << "  • Vertices/sec:    " << std::scientific << std::setprecision(2)
              << metrics.vertices_per_second << "\n";
    std::cout << "  • Triangles/sec:   " << std::scientific << std::setprecision(2)
              << metrics.triangles_per_second << "\n";
    std::cout << "  • MPixels/sec:     " << std::fixed << std::setprecision(2)
              << (metrics.pixels_per_second / 1000000.0) << "\n";
}

void test_basic_rendering(TinyGLTestFramework& framework) {
    print_separator("Basic Rendering Test");

    // Test configuration
    SceneConfig config;
    config.width = 800;
    config.height = 600;
    config.enable_lighting = true;
    config.enable_smooth_shading = true;

    // Test 1: Sphere with varying tessellation
    std::cout << "\n🔵 Testing sphere tessellation levels...\n";

    for (int subdiv = 0; subdiv <= 3; subdiv++) {
        SphereTesselationScene sphere(subdiv);
        framework.render_scene(&sphere, config);

        std::string filename =
            "tests/tinygl/output/sphere_subdiv_" + std::to_string(subdiv) + ".ppm";
        framework.save_screenshot(filename);

        // Benchmark
        auto perf = framework.benchmark_scene(&sphere, config, 100);
        std::cout << "  Subdivision " << subdiv << ": " << sphere.get_triangle_count()
                  << " triangles, " << std::fixed << std::setprecision(2) << perf.frame_time_ms
                  << " ms/frame\n";
    }
}

void test_gouraud_artifacts(TinyGLTestFramework& framework) {
    print_separator("Gouraud Shading Artifacts Test");

    SceneConfig config;
    config.enable_smooth_shading = true;

    GouraudArtifactScene scene;
    framework.render_scene(&scene, config);
    framework.save_screenshot("tests/tinygl/output/gouraud_artifacts.ppm");

    if (g_verify_mode) {
        verify_rendering(framework, "Gouraud_Artifacts",
                         "tests/tinygl/reference/Gouraud_Artifacts.ppm");
    } else {
        std::cout << "\n🎨 Gouraud artifact test rendered.\n";
        std::cout << "  Low-tessellation cylinder should show clear faceting.\n";
        std::cout << "  High-tessellation cylinder should appear smoother.\n";
    }
}

void test_color_banding(TinyGLTestFramework& framework) {
    print_separator("Color Banding Test");

    SceneConfig config;
    config.enable_lighting = true;
    config.enable_smooth_shading = true;
    config.ambient_intensity = 0.3f;
    config.specular_intensity = 0.05f;

    ColorBandingScene scene;
    framework.render_scene(&scene, config);
    framework.save_screenshot("tests/tinygl/output/color_banding.ppm");

    if (g_verify_mode) {
        verify_rendering(framework, "Color_Banding", "tests/tinygl/reference/Color_Banding.ppm");
    } else {
        std::cout << "\n🌈 Color banding test rendered.\n";
        std::cout << "  Gradient should show visible 8-bit quantization bands.\n";
        std::cout << "  Sphere lighting should show subtle banding in shadows.\n";
    }
}

void test_performance_scaling(TinyGLTestFramework& framework) {
    print_separator("Performance Scaling Test");

    SceneConfig config;

    std::cout << "\n📈 Testing performance with increasing complexity...\n\n";

    // Test cube grids of increasing size
    for (int size = 2; size <= 8; size += 2) {
        CubeGridScene scene(size);
        auto perf = framework.benchmark_scene(&scene, config, 50);

        std::cout << "  Grid " << size << "×" << size << "×" << size << " ("
                  << scene.get_triangle_count() << " triangles): " << std::fixed
                  << std::setprecision(2) << perf.frame_time_ms << " ms, " << std::setprecision(1)
                  << (1000.0 / perf.frame_time_ms) << " FPS\n";
    }
}

void test_lighting_configurations(TinyGLTestFramework& framework) {
    print_separator("Lighting Configuration Test");

    SphereTesselationScene sphere(3);

    // Test different lighting setups
    std::vector<std::pair<std::string, SceneConfig>> configs = {
        {"no_lighting", {800, 600, true, false}},
        {"flat_shading", {800, 600, true, true, false, false}},
        {"gouraud_1_light", {800, 600, true, true, false, true, 1}},
        {"gouraud_2_lights", {800, 600, true, true, false, true, 2}},
        {"high_specular", {800, 600, true, true, false, true, 2, 0.3f, 0.5f, 128.0f}},
    };

    std::cout << "\n💡 Testing lighting configurations...\n\n";

    for (const auto& [name, config] : configs) {
        framework.render_scene(&sphere, config);
        std::string filename = "tests/tinygl/output/lighting_" + name + ".ppm";
        framework.save_screenshot(filename);

        auto perf = framework.benchmark_scene(&sphere, config, 50);
        std::cout << "  " << std::setw(20) << std::left << name << ": " << std::fixed
                  << std::setprecision(2) << perf.frame_time_ms << " ms/frame\n";
    }
}

void generate_reference_images(TinyGLTestFramework& framework) {
    print_separator("Generating Reference Images");

    SceneConfig config;
    config.enable_lighting = true;
    config.enable_smooth_shading = true;
    config.ambient_intensity = 0.3f;
    config.specular_intensity = 0.05f;

    // Generate reference images for all test scenes
    std::vector<std::unique_ptr<TestScene>> scenes;
    scenes.push_back(std::make_unique<SphereTesselationScene>(3));
    scenes.push_back(std::make_unique<CubeGridScene>(4));
    scenes.push_back(std::make_unique<GouraudArtifactScene>());
    scenes.push_back(std::make_unique<ColorBandingScene>());

    std::cout << "\n📸 Generating reference images...\n";

    for (auto& scene : scenes) {
        framework.render_scene(scene.get(), config);
        std::string filename = "tests/tinygl/reference/" + scene->get_name() + ".ppm";
        // Replace spaces with underscores
        std::replace(filename.begin(), filename.end(), ' ', '_');
        framework.save_screenshot(filename);
        std::cout << "  ✓ " << scene->get_name() << "\n";
    }
}

void print_test_summary() {
    print_separator("Test Summary");

    int passed = 0;
    int failed = 0;

    for (const auto& result : g_test_results) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }

    std::cout << "\n";
    std::cout << "  Total tests:   " << g_test_results.size() << "\n";
    std::cout << "  ✅ Passed:      " << passed << "\n";
    std::cout << "  ❌ Failed:      " << failed << "\n";
    std::cout << "\n";

    if (failed > 0) {
        std::cout << "Failed tests:\n";
        for (const auto& result : g_test_results) {
            if (!result.passed) {
                std::cout << "  • " << result.test_name;
                if (!result.failure_reason.empty()) {
                    std::cout << " (" << result.failure_reason << ")";
                }
                std::cout << "\n";
            }
        }
        std::cout << "\n";
    }
}

int main(int argc, char** argv) {
    // Set up logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] %v");

    // Parse command line arguments
    bool verify_mode = false;
    std::string test_name = "all";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--verify") {
            verify_mode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "TinyGL Test Framework\n\n";
            std::cout << "Usage: " << argv[0] << " [test_name] [--verify]\n\n";
            std::cout << "Test names:\n";
            std::cout << "  all         - Run all tests (default)\n";
            std::cout << "  basic       - Basic rendering tests\n";
            std::cout << "  gouraud     - Gouraud shading artifacts\n";
            std::cout << "  banding     - Color banding tests\n";
            std::cout << "  performance - Performance benchmarks\n";
            std::cout << "  lighting    - Lighting configuration tests\n";
            std::cout << "  reference   - Generate reference images\n\n";
            std::cout << "Options:\n";
            std::cout << "  --verify    - Verify rendering against reference images\n";
            std::cout << "                Returns exit code 0 (pass) or 1 (fail)\n\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << "                    # Run all tests\n";
            std::cout << "  " << argv[0] << " --verify           # Verify all tests\n";
            std::cout << "  " << argv[0] << " gouraud --verify   # Verify Gouraud test\n";
            std::cout << "  " << argv[0] << " reference          # Generate references\n";
            return 0;
        } else {
            test_name = arg;
        }
    }

    print_separator("TinyGL Test Framework");
    std::cout << "\n";
    std::cout << "  Testing TinyGL rendering quality and performance\n";
    std::cout << "  Output directory: tests/tinygl/output/\n";
    if (verify_mode) {
        std::cout << "  Mode: VERIFICATION (comparing against reference images)\n";
    }
    std::cout << "\n";

    // Create output directories
    std::filesystem::create_directories("tests/tinygl/output");
    std::filesystem::create_directories("tests/tinygl/reference");

    // Initialize test framework
    TinyGLTestFramework framework(800, 600);
    if (!framework.initialize()) {
        spdlog::error("Failed to initialize TinyGL test framework");
        return 1;
    }

    // Set global verify mode flag
    g_verify_mode = verify_mode;

    // Run test suites
    if (test_name == "basic") {
        test_basic_rendering(framework);
    } else if (test_name == "gouraud") {
        test_gouraud_artifacts(framework);
    } else if (test_name == "banding") {
        test_color_banding(framework);
    } else if (test_name == "performance") {
        test_performance_scaling(framework);
    } else if (test_name == "lighting") {
        test_lighting_configurations(framework);
    } else if (test_name == "reference") {
        generate_reference_images(framework);
    } else if (test_name == "all") {
        // Run all tests (but not reference generation)
        test_basic_rendering(framework);
        test_gouraud_artifacts(framework);
        test_color_banding(framework);
        test_lighting_configurations(framework);
        if (!verify_mode) {
            test_performance_scaling(framework);
        }
    } else {
        std::cout << "Unknown test: " << test_name << "\n";
        std::cout
            << "Available tests: all, basic, gouraud, banding, performance, lighting, reference\n";
        std::cout << "Run with --help for full usage information\n";
        return 1;
    }

    // Print summary and return appropriate exit code
    if (verify_mode && !g_test_results.empty()) {
        print_test_summary();

        int failed_count = 0;
        for (const auto& result : g_test_results) {
            if (!result.passed)
                failed_count++;
        }

        print_separator();
        if (failed_count == 0) {
            std::cout << "\n✅ All verification tests PASSED!\n\n";
            return 0;
        } else {
            std::cout << "\n❌ " << failed_count << " test(s) FAILED!\n\n";
            return 1;
        }
    } else {
        print_separator();
        std::cout << "\n✅ All tests completed!\n";
        std::cout << "\nView results:\n";
        std::cout << "  • macOS: open tests/tinygl/output/*.ppm\n";
        std::cout << "  • Linux: xdg-open tests/tinygl/output/*.ppm\n";
        std::cout << "\n";
        return 0;
    }
}