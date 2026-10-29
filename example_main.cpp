/*
 * hexmatch.hpp - Complete Example
 *
 * This example demonstrates all features of the pattern matching library.
 * Compile with C++20: cl /std:c++20 /arch:AVX2 /EHsc example_main.cpp
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>
#include <chrono>
#include <intrin.h>

// Include the header-only library
#include "hexmatch.hpp"

// ============================================================================
// Helper Functions
// ============================================================================

// Check if CPU supports AVX2
bool is_avx2_supported() {
    int cpu_info[4];
    __cpuid(cpu_info, 0);
    if (cpu_info[0] >= 7) {
        __cpuidex(cpu_info, 7, 0);
        return (cpu_info[1] & (1 << 5)) != 0;  // EBX bit 5
    }
    return false;
}

// Create a test memory buffer with known patterns
std::vector<uint8_t> create_test_buffer() {
    std::vector<uint8_t> buffer(1024 * 1024);  // 1 MB test buffer

    // Fill with random-ish data
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = static_cast<uint8_t>(i ^ (i >> 8));
    }

    // Insert known patterns at specific locations

    // Pattern 1: Simple byte sequence at offset 0x1000
    const uint8_t pattern1[] = {0x48, 0x8B, 0x05, 0xAA, 0xBB, 0xCC, 0xDD};
    std::memcpy(&buffer[0x1000], pattern1, sizeof(pattern1));

    // Pattern 2: CALL instruction at offset 0x2000
    const uint8_t pattern2[] = {0xE8, 0x10, 0x20, 0x30, 0x40};
    std::memcpy(&buffer[0x2000], pattern2, sizeof(pattern2));

    // Pattern 3: MOV EAX, immediate at offset 0x3000
    const uint8_t pattern3[] = {0xB8, 0x78, 0x56, 0x34, 0x12};
    std::memcpy(&buffer[0x3000], pattern3, sizeof(pattern3));

    // Pattern 4: Repeated pattern (3 occurrences)
    const uint8_t pattern4[] = {0x48, 0x89, 0x5C, 0x24, 0x08};
    std::memcpy(&buffer[0x4000], pattern4, sizeof(pattern4));
    std::memcpy(&buffer[0x4100], pattern4, sizeof(pattern4));
    std::memcpy(&buffer[0x4200], pattern4, sizeof(pattern4));

    // Pattern 5: Long pattern for SIMD testing
    const uint8_t pattern5[] = {
        0x55, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC, 0x20,
        0x48, 0x89, 0x4C, 0x24, 0x08, 0x48, 0x8B, 0x44,
        0x24, 0x08, 0x48, 0x83, 0xC4, 0x20, 0x5D, 0xC3
    };
    std::memcpy(&buffer[0x5000], pattern5, sizeof(pattern5));

    return buffer;
}

// Print buffer hex dump
void print_hex_dump(const uint8_t* data, size_t offset, size_t length) {
    std::cout << "Hex dump at offset 0x" << std::hex << std::setfill('0')
              << std::setw(4) << offset << ":\n";

    for (size_t i = 0; i < length; ++i) {
        if (i % 16 == 0) {
            std::cout << std::setw(4) << (offset + i) << ":  ";
        }
        std::cout << std::setw(2) << static_cast<int>(data[offset + i]) << " ";
        if ((i + 1) % 16 == 0 || i == length - 1) {
            std::cout << "\n";
        }
    }
    std::cout << std::dec << std::endl;
}

// Measure execution time
template <typename Func>
double measure_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// ============================================================================
// Example Functions
// ============================================================================

// Example 1: Basic pattern matching
void example1_basic_search(const std::vector<uint8_t>& buffer) {
    std::cout << "=== Example 1: Basic Pattern Search ===\n";

    const uint8_t* start = buffer.data();
    const uint8_t* end = buffer.data() + buffer.size();

    void* result = nullptr;
    bool not_found = SearchPatternSSE2(
        reinterpret_cast<std::intptr_t>(start),
        reinterpret_cast<std::intptr_t>(end),
        "48 8B 05 ?? ?? ?? ??",  // Pattern with wildcards
        1,                        // Find first match
        0,                        // Offset 0
        0,                        // Size (not used with math_address)
        math_address,             // Return address of match
        result
    );

    if (!not_found) {
        size_t offset = static_cast<uint8_t*>(result) - start;
        std::cout << "✓ Pattern found at offset: 0x" << std::hex << offset << std::dec << "\n";
        print_hex_dump(start, offset, 16);
    } else {
        std::cout << "✗ Pattern not found\n";
    }
    std::cout << std::endl;
}

// Example 2: Extract immediate value
void example2_extract_immediate(const std::vector<uint8_t>& buffer) {
    std::cout << "=== Example 2: Extract Immediate Value ===\n";

    const uint8_t* start = buffer.data();
    const uint8_t* end = buffer.data() + buffer.size();

    // Pattern: B8 [XX XX XX XX] (MOV EAX, imm32)
    int32_t immediate = 0;
    bool not_found = SearchPatternSSE2(
        reinterpret_cast<std::intptr_t>(start),
        reinterpret_cast<std::intptr_t>(end),
        "B8 ?? ?? ?? ??",
        1,
        1,                // Offset 1 (after B8)
        4,                // Read 4 bytes
        math_constant,    // Read raw value
        immediate
    );

    if (!not_found) {
        std::cout << "✓ Immediate value: 0x" << std::hex << immediate << std::dec << "\n";
        std::cout << "  (Expected: 0x12345678)\n";
    } else {
        std::cout << "✗ Pattern not found\n";
    }
    std::cout << std::endl;
}

// Example 3: Find Nth occurrence
void example3_nth_match(const std::vector<uint8_t>& buffer) {
    std::cout << "=== Example 3: Find Nth Occurrence ===\n";

    const uint8_t* start = buffer.data();
    const uint8_t* end = buffer.data() + buffer.size();

    // Find all 3 occurrences
    for (size_t n = 1; n <= 3; ++n) {
        void* result = nullptr;
        bool not_found = SearchPatternSSE2(
            reinterpret_cast<std::intptr_t>(start),
            reinterpret_cast<std::intptr_t>(end),
            "48 89 5C 24 08",
            n,  // Find Nth match
            0, 0, math_address, result
        );

        if (!not_found) {
            size_t offset = static_cast<uint8_t*>(result) - start;
            std::cout << "✓ Match #" << n << " at offset: 0x" << std::hex << offset << std::dec << "\n";
        } else {
            std::cout << "✗ Match #" << n << " not found\n";
        }
    }
    std::cout << std::endl;
}

// Example 4: RIP-relative address calculation (math_imm)
void example4_rip_relative(const std::vector<uint8_t>& buffer) {
    std::cout << "=== Example 4: RIP-Relative Address (x86_64) ===\n";

    const uint8_t* start = buffer.data();
    const uint8_t* end = buffer.data() + buffer.size();

    // Pattern: E8 [XX XX XX XX] (CALL rel32)
    void* call_target = nullptr;
    bool not_found = SearchPatternSSE2(
        reinterpret_cast<std::intptr_t>(start),
        reinterpret_cast<std::intptr_t>(end),
        "E8 ?? ?? ?? ??",
        1,
        1,             // Offset to rel32
        4,             // 4-byte offset
        math_imm,      // Calculate absolute address
        call_target
    );

    if (!not_found) {
        std::cout << "✓ CALL target calculated: 0x" << call_target << "\n";
        std::cout << "  (This is: match_pos + 1 + 4 + rel32_value)\n";
    } else {
        std::cout << "✗ Pattern not found\n";
    }
    std::cout << std::endl;
}

// Example 5: Compare performance SSE2 vs AVX2
void example5_performance_comparison(const std::vector<uint8_t>& buffer) {
    std::cout << "=== Example 5: Performance Comparison ===\n";

    const uint8_t* start = buffer.data();
    const uint8_t* end = buffer.data() + buffer.size();
    const std::string pattern = "55 48 8B EC 48 83 EC 20 48 89 4C 24 08";
    const int iterations = 100;

    // Baseline: SearchPattern
    void* result = nullptr;
    double time_baseline = measure_time([&]() {
        for (int i = 0; i < iterations; ++i) {
            SearchPattern(reinterpret_cast<std::intptr_t>(start),
                         reinterpret_cast<std::intptr_t>(end),
                         pattern, 1, 0, 0, math_address, result);
        }
    });

    // SSE2 version
    double time_sse2 = measure_time([&]() {
        for (int i = 0; i < iterations; ++i) {
            SearchPatternSSE2(reinterpret_cast<std::intptr_t>(start),
                             reinterpret_cast<std::intptr_t>(end),
                             pattern, 1, 0, 0, math_address, result);
        }
    });

    // AVX2 version (if supported)
    double time_avx2 = 0.0;
    if (is_avx2_supported()) {
        time_avx2 = measure_time([&]() {
            for (int i = 0; i < iterations; ++i) {
                SearchPatternAVX2(reinterpret_cast<std::intptr_t>(start),
                                 reinterpret_cast<std::intptr_t>(end),
                                 pattern, 1, 0, 0, math_address, result);
            }
        });
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Buffer size: " << buffer.size() / 1024 << " KB\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    std::cout << "SearchPattern (baseline): " << time_baseline << " ms\n";
    std::cout << "SearchPatternSSE2:        " << time_sse2 << " ms (";
    std::cout << time_baseline / time_sse2 << "x speedup)\n";

    if (time_avx2 > 0.0) {
        std::cout << "SearchPatternAVX2:        " << time_avx2 << " ms (";
        std::cout << time_baseline / time_avx2 << "x speedup)\n";
    } else {
        std::cout << "SearchPatternAVX2:        Not supported on this CPU\n";
    }
    std::cout << std::endl;
}

// Example 6: Find all matches
void example6_find_all_matches(const std::vector<uint8_t>& buffer) {
    std::cout << "=== Example 6: Find All Matches ===\n";

    const uint8_t* start = buffer.data();
    const uint8_t* end = buffer.data() + buffer.size();

    std::vector<void*> matches;

    // Keep searching until no more matches
    for (size_t i = 1; ; ++i) {
        void* result = nullptr;
        bool not_found = SearchPatternSSE2(
            reinterpret_cast<std::intptr_t>(start),
            reinterpret_cast<std::intptr_t>(end),
            "48 89 5C 24 08",
            i,
            0, 0, math_address, result
        );

        if (not_found) {
            break;  // No more matches
        }
        matches.push_back(result);
    }

    std::cout << "✓ Found " << matches.size() << " matches:\n";
    for (size_t i = 0; i < matches.size(); ++i) {
        size_t offset = static_cast<uint8_t*>(matches[i]) - start;
        std::cout << "  #" << (i + 1) << ": 0x" << std::hex << offset << std::dec << "\n";
    }
    std::cout << std::endl;
}

// Example 7: Prefetch tuning
void example7_prefetch_tuning(const std::vector<uint8_t>& buffer) {
    std::cout << "=== Example 7: Prefetch Distance Tuning ===\n";

    const uint8_t* start = buffer.data();
    const uint8_t* end = buffer.data() + buffer.size();
    const std::string pattern = "55 48 8B EC 48 83 EC 20";
    const int iterations = 50;

    std::vector<size_t> prefetch_sizes = {64, 128, 256, 512};

    std::cout << "Testing different prefetch distances:\n";
    for (size_t pf_size : prefetch_sizes) {
        void* result = nullptr;
        double time = measure_time([&]() {
            for (int i = 0; i < iterations; ++i) {
                SearchPatternSSE2(reinterpret_cast<std::intptr_t>(start),
                                 reinterpret_cast<std::intptr_t>(end),
                                 pattern, 1, 0, 0, math_address, result, pf_size);
            }
        });

        std::cout << "  Prefetch " << std::setw(3) << pf_size << " bytes: "
                  << std::fixed << std::setprecision(2) << time << " ms\n";
    }
    std::cout << std::endl;
}

// Example 8: Error handling
void example8_error_handling(const std::vector<uint8_t>& buffer) {
    std::cout << "=== Example 8: Error Handling ===\n";

    const uint8_t* start = buffer.data();
    const uint8_t* end = buffer.data() + buffer.size();

    // Test 1: Pattern not found
    void* result = nullptr;
    bool not_found = SearchPatternSSE2(
        reinterpret_cast<std::intptr_t>(start),
        reinterpret_cast<std::intptr_t>(end),
        "FF FF FF FF FF FF FF FF",  // Pattern that doesn't exist
        1, 0, 0, math_address, result
    );

    if (not_found) {
        std::cout << "✓ Correctly detected pattern not found\n";
    } else {
        std::cout << "✗ False positive!\n";
    }

    // Test 2: Empty pattern
    not_found = SearchPatternSSE2(
        reinterpret_cast<std::intptr_t>(start),
        reinterpret_cast<std::intptr_t>(end),
        "",  // Empty pattern
        1, 0, 0, math_address, result
    );

    if (not_found) {
        std::cout << "✓ Correctly handled empty pattern\n";
    } else {
        std::cout << "✗ Should have failed on empty pattern\n";
    }

    // Test 3: Invalid range (start == end)
    not_found = SearchPatternSSE2(
        reinterpret_cast<std::intptr_t>(start),
        reinterpret_cast<std::intptr_t>(start),  // start == end
        "48 8B 05",
        1, 0, 0, math_address, result
    );

    if (not_found) {
        std::cout << "✓ Correctly handled invalid range\n";
    } else {
        std::cout << "✗ Should have failed on invalid range\n";
    }

    std::cout << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   hexmatch.hpp - Complete Example Suite                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // CPU detection
    std::cout << "CPU Features:\n";
    std::cout << "  SSE2:  ✓ (required)\n";
    std::cout << "  AVX2:  " << (is_avx2_supported() ? "✓" : "✗") << "\n\n";

    // Create test buffer
    std::cout << "Creating test buffer with known patterns...\n\n";
    auto buffer = create_test_buffer();

    // Run examples
    example1_basic_search(buffer);
    example2_extract_immediate(buffer);
    example3_nth_match(buffer);
    example4_rip_relative(buffer);
    example5_performance_comparison(buffer);
    example6_find_all_matches(buffer);
    example7_prefetch_tuning(buffer);
    example8_error_handling(buffer);

    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   All examples completed!                                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    return 0;
}
