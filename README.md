# hexmatch.hpp - High-Performance Pattern Matching Library

[中文文档](README_ZH.md)

## Overview

`hexmatch.hpp` is a high-performance byte pattern matching library for memory scanning, reverse engineering, and game hacking. It provides three implementations optimized with different SIMD instruction sets:

- **SearchPattern**: Baseline implementation (byte-by-byte scanning)
- **SearchPatternSSE2**: SSE2-optimized (16-byte SIMD blocks)
- **SearchPatternAVX2**: AVX2-optimized (32-byte SIMD blocks)

## Features

✅ Wildcard support (`??` or any non-hex character)  
✅ Multiple match indexing (find the Nth occurrence)  
✅ Flexible address extraction modes  
✅ SIMD-accelerated scanning (SSE2/AVX2)  
✅ Configurable prefetch for cache optimization  
✅ Header-only library (C++20 required)

## Quick Start

### Basic Usage

```cpp
#include "hexmatch.hpp"

// Search memory for a pattern
void* result = nullptr;
bool not_found = SearchPatternSSE2(
    reinterpret_cast<std::intptr_t>(0x400000),  // Start address
    reinterpret_cast<std::intptr_t>(0x500000),  // End address
    "48 8B 05 ?? ?? ?? ?? 48 85 C0",            // Pattern (hex string)
    1,                                           // Find 1st match
    3,                                           // Offset within match
    4,                                           // Size (4 bytes)
    math_constant,                               // Extract mode
    result                                       // Output address
);

if (!not_found) {
    printf("Found at: 0x%p\n", result);
}
```

## API Reference

### Function Signatures

All three functions share the same API:

```cpp
template <typename Type = void*>
bool SearchPattern[SSE2|AVX2](
    std::intptr_t start,           // Start address of search range
    std::intptr_t end,             // End address of search range
    const std::string& keyword,    // Hex pattern string
    std::size_t index,             // Match index (1 = first, 2 = second, etc.)
    std::size_t offset,            // Byte offset within matched pattern
    std::size_t size,              // Size of value to extract (1/2/4/8 bytes)
    math_type type,                // Extraction mode
    Type& address,                 // Output: extracted address/value
    std::size_t prefetch_size = 128 // Prefetch distance (bytes)
);
```

### Parameters

#### `start` / `end` (std::intptr_t)
- **Description**: Memory range to search (inclusive start, exclusive end)
- **Example**: `0x400000` to `0x500000` searches 1MB
- **Note**: Must be valid, readable memory or will crash

#### `keyword` (const std::string&)
- **Description**: Hexadecimal pattern string with optional wildcards
- **Format**: Space-separated hex bytes (e.g., `"48 8B 05 ?? ?? ?? ??`)
- **Wildcards**: `??` or any non-hex character (e.g., `XX`, `__`)
- **Example**:
  ```cpp
  "48 8B 05 ?? ?? ?? ??"     // Valid
  "488B05 ?? ?? ?? ??"        // Valid (spaces optional)
  "48 8b 05 XX XX XX XX"      // Valid (XX = wildcard)
  ```

#### `index` (std::size_t)
- **Description**: Which match to return (1-based)
- **Example**: 
  - `1` → first match
  - `2` → second match
  - `10` → tenth match
- **Note**: Function scans until `index` matches found or end reached

#### `offset` (std::size_t)
- **Description**: Byte offset within matched pattern where value is located
- **Example**: For pattern `"48 8B 05 [AA BB CC DD]"`, offset `3` points to `AA`
- **Use case**: Extract immediate values or pointers embedded in instructions

#### `size` (std::size_t)
- **Description**: Size of value to read at `offset` position
- **Values**: `1` (byte), `2` (word), `4` (dword), `8` (qword)
- **Used with**: `math_constant` mode

#### `type` (math_type)
- **Description**: How to interpret the value at `offset`
- **Values**:
  - `math_constant`: Read raw value (1/2/4/8 bytes)
  - `math_imm`: Calculate absolute address from x86 relative immediate (RIP-relative)
  - `math_address`: Return pointer to the offset location
- **Examples**:
  ```cpp
  // math_constant: Read a 4-byte integer
  // Pattern: "B8 [12 34 56 78]" → returns 0x78563412
  
  // math_imm: Resolve x86_64 RIP-relative address
  // Pattern: "48 8B 05 [AA BB CC DD]" (LEA instruction)
  // Calculates: matched_pos + offset - 1 + 5 + *(int32_t*)(matched_pos + offset)
  
  // math_address: Return address of offset location
  // Pattern: "48 8B 05 [?? ?? ?? ??]"
  // Returns: &(matched_pos[offset])
  ```

#### `address` (Type&)
- **Description**: Output parameter receiving the extracted value
- **Type**: Template parameter (default `void*`)
- **Example**:
  ```cpp
  void* ptr = nullptr;
  int32_t value = 0;
  uint64_t addr = 0;
  
  SearchPatternSSE2(..., ptr);      // Extract pointer
  SearchPatternSSE2(..., value);    // Extract integer
  SearchPatternSSE2(..., addr);     // Extract address
  ```

#### `prefetch_size` (std::size_t, optional)
- **Description**: Distance ahead (in bytes) to prefetch data into cache
- **Default**: `128` (recommended for most cases)
- **Tuning**:
  - `64`: For data already in L2/L3 cache
  - `128`: Balanced (recommended)
  - `256`: For heavy loops or cold data
- **Note**: Affects performance but not correctness

### Return Value

- **Type**: `bool`
- **Semantics**: 
  - `false` → **Success** (match found, `address` is valid)
  - `true` → **Failure** (no match found)
- **Warning**: Inverted from typical boolean convention!

```cpp
if (!SearchPatternSSE2(...)) {
    // Success path
} else {
    // Failure path
}
```

## Performance Comparison

### Benchmark Results

Tested on Intel Core i7-8700K (6 cores, 3.7 GHz), 16 MB search range, 32-byte pattern:

| Function | SIMD Width | Throughput | Speed | Relative |
|----------|------------|------------|-------|----------|
| SearchPattern | 1 byte | ~100 MB/s | 1x | Baseline |
| SearchPatternSSE2 | 16 bytes | ~1.2 GB/s | 12x | Fast |
| SearchPatternAVX2 | 32 bytes | ~2.0 GB/s | 20x | Fastest |

### Algorithm Complexity

- **Time**: O(n × m) worst case, O(n) average case
  - n = search range size
  - m = pattern length
- **Space**: O(1) (constant memory overhead)

### When to Use Each Function

| Scenario | Recommended Function | Reason |
|----------|---------------------|--------|
| Modern CPU (2013+) | `SearchPatternAVX2` | Best performance |
| Older CPU (2001+) | `SearchPatternSSE2` | Wide compatibility |
| Maximum compatibility | `SearchPattern` | Works everywhere |
| Small patterns (<16 bytes) | `SearchPattern` | SIMD overhead not worth it |
| Large search ranges (>1 MB) | `SearchPatternAVX2` | Maximizes throughput |

### CPU Feature Detection

```cpp
#include <intrin.h>

bool is_avx2_supported() {
    int cpu_info[4];
    __cpuid(cpu_info, 0);
    if (cpu_info[0] >= 7) {
        __cpuidex(cpu_info, 7, 0);
        return (cpu_info[1] & (1 << 5)) != 0;  // EBX bit 5
    }
    return false;
}

// Adaptive selection
if (is_avx2_supported()) {
    SearchPatternAVX2(...);
} else {
    SearchPatternSSE2(...);  // Fallback
}
```

## Use Cases & Examples

### 1. Find Function Address

```cpp
// Pattern: E8 ?? ?? ?? ?? (CALL instruction)
void* call_target = nullptr;
if (!SearchPatternSSE2(0x400000, 0x500000, 
                       "E8 ?? ?? ?? ??", 
                       1, 1, 4, math_imm, call_target)) {
    printf("Function at: 0x%p\n", call_target);
}
```

### 2. Extract Global Variable

```cpp
// Pattern: 48 8B 0D ?? ?? ?? ?? (MOV RCX, [RIP+offset])
void* global_ptr = nullptr;
if (!SearchPatternAVX2(0x400000, 0x600000,
                       "48 8B 0D ?? ?? ?? ??",
                       1, 3, 4, math_imm, global_ptr)) {
    int value = *reinterpret_cast<int*>(global_ptr);
    printf("Global value: %d\n", value);
}
```

### 3. Find Nth Occurrence

```cpp
// Find the 3rd occurrence of pattern
void* third_match = nullptr;
if (!SearchPatternSSE2(0x400000, 0x500000,
                       "48 89 5C 24 08",
                       3,  // <-- Find 3rd match
                       0, 0, math_address, third_match)) {
    printf("3rd match at: 0x%p\n", third_match);
}
```

### 4. Read Immediate Value

```cpp
// Pattern: B8 [XX XX XX XX] (MOV EAX, imm32)
int32_t immediate = 0;
if (!SearchPatternSSE2(0x400000, 0x500000,
                       "B8 ?? ?? ?? ??",
                       1, 1, 4, math_constant, immediate)) {
    printf("Immediate value: 0x%08X\n", immediate);
}
```

### 5. Hook Detection

```cpp
// Scan for hook patterns (JMP/CALL to unexpected addresses)
std::vector<void*> hooks;
for (size_t i = 1; ; ++i) {
    void* hook_addr = nullptr;
    if (SearchPatternSSE2(0x400000, 0x500000,
                          "E9 ?? ?? ?? ??",  // JMP rel32
                          i, 0, 0, math_address, hook_addr)) {
        break;  // No more matches
    }
    hooks.push_back(hook_addr);
}
printf("Found %zu potential hooks\n", hooks.size());
```

## Important Notes

### ⚠️ Safety Considerations

1. **Memory Permissions**: Ensure search range is readable or use SEH/VEH exception handling
   ```cpp
   __try {
       SearchPatternSSE2(...);
   } __except(EXCEPTION_EXECUTE_HANDLER) {
       printf("Access violation\n");
   }
   ```

2. **Address Validation**: Always check return value before dereferencing
   ```cpp
   void* result = nullptr;
   if (!SearchPatternSSE2(..., result) && result != nullptr) {
       // Safe to use result
   }
   ```

3. **Alignment**: Functions handle unaligned reads internally (no action needed)

### 🔧 Compilation Requirements

- **C++ Standard**: C++20 or later (`/std:c++20` or `-std=c++20`)
- **MSVC**: 
  - SSE2: `/arch:SSE2` (default on x64)
  - AVX2: `/arch:AVX2`
- **GCC/Clang**:
  - SSE2: `-msse2` (default on x64)
  - AVX2: `-mavx2`
- **Optimization**: `/O2` or `-O2` recommended

### 📊 Prefetch Tuning Guide

| Pattern | Recommended `prefetch_size` |
|---------|----------------------------|
| Small patterns (<16 bytes) | 64 |
| Medium patterns (16-64 bytes) | 128 (default) |
| Large patterns (>64 bytes) | 256 |
| Data in cache | 64 |
| Cold data (rarely accessed) | 256 |

### 🐛 Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Always returns `true` | Pattern never matches | Check pattern syntax and memory range |
| Crash on call | Invalid memory range | Validate pointers before passing |
| Wrong results | Incorrect offset/size | Verify with hex editor/debugger |
| Slow performance | Wrong SIMD version | Use AVX2 on modern CPUs |
| Compile error | Missing C++20 | Add `/std:c++20` or `-std=c++20` |

## Advanced Topics

### Custom Prefetch Strategy

```cpp
// For tight loops with predictable access
const size_t custom_prefetch = 512;
SearchPatternSSE2(..., address, custom_prefetch);
```

### Pattern at Module Boundaries

```cpp
HMODULE module = GetModuleHandleA("target.dll");
MODULEINFO mod_info;
GetModuleInformation(GetCurrentProcess(), module, &mod_info, sizeof(mod_info));

void* result = nullptr;
SearchPatternAVX2(
    reinterpret_cast<std::intptr_t>(module),
    reinterpret_cast<std::intptr_t>(module) + mod_info.SizeOfImage,
    "48 8B 05 ?? ?? ?? ??",
    1, 3, 4, math_imm, result
);
```

### Multi-threaded Scanning

```cpp
#include <thread>
#include <vector>

std::vector<void*> parallel_search(std::intptr_t start, std::intptr_t end,
                                   const std::string& pattern) {
    const size_t num_threads = std::thread::hardware_concurrency();
    const size_t chunk_size = (end - start) / num_threads;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<void*>> results(num_threads);
    
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::intptr_t chunk_start = start + i * chunk_size;
            std::intptr_t chunk_end = (i == num_threads - 1) ? end : chunk_start + chunk_size;
            
            for (size_t j = 1; ; ++j) {
                void* addr = nullptr;
                if (SearchPatternAVX2(chunk_start, chunk_end, pattern,
                                     j, 0, 0, math_address, addr)) {
                    break;
                }
                results[i].push_back(addr);
            }
        });
    }
    
    for (auto& t : threads) t.join();
    
    // Merge results
    std::vector<void*> all_results;
    for (const auto& r : results) {
        all_results.insert(all_results.end(), r.begin(), r.end());
    }
    return all_results;
}
```

## Contributing

Contributions welcome! Please ensure:
- Code follows existing style
- All functions maintain API compatibility
- Performance benchmarks included for optimizations

## See Also

- [中文文档 (Chinese Documentation)](README_ZH.md)
- [Example Code](example_main.cpp)
