# hexmatch.hpp - 高性能模式匹配库

[English Documentation](README.md)

## 概述

`hexmatch.hpp` 是一个用于内存扫描、逆向工程和游戏修改的高性能字节模式匹配库。它提供了三种使用不同 SIMD 指令集优化的实现：

- **SearchPattern**: 基线实现（逐字节扫描）
- **SearchPatternSSE2**: SSE2 优化（16 字节 SIMD 块）
- **SearchPatternAVX2**: AVX2 优化（32 字节 SIMD 块）

## 特性

✅ 通配符支持（`??` 或任何非十六进制字符）  
✅ 多重匹配索引（查找第 N 个匹配项）  
✅ 灵活的地址提取模式  
✅ SIMD 加速扫描（SSE2/AVX2）  
✅ 可配置的预取以优化缓存  
✅ 仅头文件库（需要 C++20）

## 快速开始

### 基本用法

```cpp
#include "hexmatch.hpp"

// 在内存中搜索模式
void* result = nullptr;
bool not_found = SearchPatternSSE2(
    reinterpret_cast<std::intptr_t>(0x400000),  // 起始地址
    reinterpret_cast<std::intptr_t>(0x500000),  // 结束地址
    "48 8B 05 ?? ?? ?? ?? 48 85 C0",            // 模式（十六进制字符串）
    1,                                           // 查找第 1 个匹配
    3,                                           // 匹配内的偏移量
    4,                                           // 大小（4 字节）
    math_constant,                               // 提取模式
    result                                       // 输出地址
);

if (!not_found) {
    printf("找到位置: 0x%p\n", result);
}
```

## API 参考

### 函数签名

三个函数共享相同的 API：

```cpp
template <typename Type = void*>
bool SearchPattern[SSE2|AVX2](
    std::intptr_t start,           // 搜索范围的起始地址
    std::intptr_t end,             // 搜索范围的结束地址
    const std::string& keyword,    // 十六进制模式字符串
    std::size_t index,             // 匹配索引（1 = 第一个，2 = 第二个，等等）
    std::size_t offset,            // 匹配模式内的字节偏移量
    std::size_t size,              // 要提取的值的大小（1/2/4/8 字节）
    math_type type,                // 提取模式
    Type& address,                 // 输出：提取的地址/值
    std::size_t prefetch_size = 128 // 预取距离（字节）
);
```

### 参数说明

#### `start` / `end` (std::intptr_t)
- **描述**: 要搜索的内存范围（包含起始，不包含结束）
- **示例**: `0x400000` 到 `0x500000` 搜索 1MB
- **注意**: 必须是有效的可读内存，否则会崩溃

#### `keyword` (const std::string&)
- **描述**: 带可选通配符的十六进制模式字符串
- **格式**: 空格分隔的十六进制字节（例如 `"48 8B 05 ?? ?? ?? ??"`）
- **通配符**: `??` 或任何非十六进制字符（例如 `XX`、`__`）
- **示例**:
  ```cpp
  "48 8B 05 ?? ?? ?? ??"     // 有效
  "488B05 ?? ?? ?? ??"        // 有效（空格可选）
  "48 8b 05 XX XX XX XX"      // 有效（XX = 通配符）
  ```

#### `index` (std::size_t)
- **描述**: 返回第几个匹配（从 1 开始）
- **示例**: 
  - `1` → 第一个匹配
  - `2` → 第二个匹配
  - `10` → 第十个匹配
- **注意**: 函数会扫描直到找到 `index` 个匹配或到达结束

#### `offset` (std::size_t)
- **描述**: 匹配模式内值所在的字节偏移量
- **示例**: 对于模式 `"48 8B 05 [AA BB CC DD]"`，偏移 `3` 指向 `AA`
- **用途**: 提取嵌入在指令中的立即数或指针

#### `size` (std::size_t)
- **描述**: 在 `offset` 位置读取的值的大小
- **值**: `1`（字节）、`2`（字）、`4`（双字）、`8`（四字）
- **配合使用**: `math_constant` 模式

#### `type` (math_type)
- **描述**: 如何解释 `offset` 处的值
- **值**:
  - `math_constant`: 读取原始值（1/2/4/8 字节）
  - `math_imm`: 从 x86 相对立即数计算绝对地址（RIP 相对）
  - `math_address`: 返回偏移位置的指针
- **示例**:
  ```cpp
  // math_constant: 读取 4 字节整数
  // 模式: "B8 [12 34 56 78]" → 返回 0x78563412
  
  // math_imm: 解析 x86_64 RIP 相对地址
  // 模式: "48 8B 05 [AA BB CC DD]" (LEA 指令)
  // 计算: matched_pos + offset - 1 + 5 + *(int32_t*)(matched_pos + offset)
  
  // math_address: 返回偏移位置的地址
  // 模式: "48 8B 05 [?? ?? ?? ??]"
  // 返回: &(matched_pos[offset])
  ```

#### `address` (Type&)
- **描述**: 接收提取值的输出参数
- **类型**: 模板参数（默认 `void*`）
- **示例**:
  ```cpp
  void* ptr = nullptr;
  int32_t value = 0;
  uint64_t addr = 0;
  
  SearchPatternSSE2(..., ptr);      // 提取指针
  SearchPatternSSE2(..., value);    // 提取整数
  SearchPatternSSE2(..., addr);     // 提取地址
  ```

#### `prefetch_size` (std::size_t, 可选)
- **描述**: 提前预取数据到缓存的距离（字节）
- **默认值**: `128`（大多数情况下推荐）
- **调优**:
  - `64`: 数据已在 L2/L3 缓存中
  - `128`: 平衡（推荐）
  - `256`: 用于重循环或冷数据
- **注意**: 影响性能但不影响正确性

### 返回值

- **类型**: `bool`
- **语义**: 
  - `false` → **成功**（找到匹配，`address` 有效）
  - `true` → **失败**（未找到匹配）
- **警告**: 与典型布尔约定相反！

```cpp
if (!SearchPatternSSE2(...)) {
    // 成功路径
} else {
    // 失败路径
}
```

## 性能对比

### 基准测试结果

在 Intel Core i7-8700K（6 核心，3.7 GHz）上测试，16 MB 搜索范围，32 字节模式：

| 函数 | SIMD 宽度 | 吞吐量 | 速度 | 相对性能 |
|----------|------------|------------|-------|----------|
| SearchPattern | 1 字节 | ~100 MB/s | 1x | 基线 |
| SearchPatternSSE2 | 16 字节 | ~1.2 GB/s | 12x | 快速 |
| SearchPatternAVX2 | 32 字节 | ~2.0 GB/s | 20x | 最快 |

### 算法复杂度

- **时间**: 最坏 O(n × m)，平均 O(n)
  - n = 搜索范围大小
  - m = 模式长度
- **空间**: O(1)（常量内存开销）

### 何时使用每个函数

| 场景 | 推荐函数 | 原因 |
|----------|---------------------|--------|
| 现代 CPU（2013+） | `SearchPatternAVX2` | 最佳性能 |
| 较老 CPU（2001+） | `SearchPatternSSE2` | 广泛兼容 |
| 最大兼容性 | `SearchPattern` | 到处都能用 |
| 小模式（<16 字节） | `SearchPattern` | SIMD 开销不值得 |
| 大搜索范围（>1 MB） | `SearchPatternAVX2` | 最大化吞吐量 |

### CPU 特性检测

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

// 自适应选择
if (is_avx2_supported()) {
    SearchPatternAVX2(...);
} else {
    SearchPatternSSE2(...);  // 回退
}
```

## 使用案例与示例

### 1. 查找函数地址

```cpp
// 模式: E8 ?? ?? ?? ?? (CALL 指令)
void* call_target = nullptr;
if (!SearchPatternSSE2(0x400000, 0x500000, 
                       "E8 ?? ?? ?? ??", 
                       1, 1, 4, math_imm, call_target)) {
    printf("函数位于: 0x%p\n", call_target);
}
```

### 2. 提取全局变量

```cpp
// 模式: 48 8B 0D ?? ?? ?? ?? (MOV RCX, [RIP+offset])
void* global_ptr = nullptr;
if (!SearchPatternAVX2(0x400000, 0x600000,
                       "48 8B 0D ?? ?? ?? ??",
                       1, 3, 4, math_imm, global_ptr)) {
    int value = *reinterpret_cast<int*>(global_ptr);
    printf("全局变量值: %d\n", value);
}
```

### 3. 查找第 N 个匹配

```cpp
// 查找模式的第 3 个匹配
void* third_match = nullptr;
if (!SearchPatternSSE2(0x400000, 0x500000,
                       "48 89 5C 24 08",
                       3,  // <-- 查找第 3 个匹配
                       0, 0, math_address, third_match)) {
    printf("第 3 个匹配位于: 0x%p\n", third_match);
}
```

### 4. 读取立即数

```cpp
// 模式: B8 [XX XX XX XX] (MOV EAX, imm32)
int32_t immediate = 0;
if (!SearchPatternSSE2(0x400000, 0x500000,
                       "B8 ?? ?? ?? ??",
                       1, 1, 4, math_constant, immediate)) {
    printf("立即数值: 0x%08X\n", immediate);
}
```

### 5. Hook 检测

```cpp
// 扫描 hook 模式（JMP/CALL 到意外地址）
std::vector<void*> hooks;
for (size_t i = 1; ; ++i) {
    void* hook_addr = nullptr;
    if (SearchPatternSSE2(0x400000, 0x500000,
                          "E9 ?? ?? ?? ??",  // JMP rel32
                          i, 0, 0, math_address, hook_addr)) {
        break;  // 没有更多匹配
    }
    hooks.push_back(hook_addr);
}
printf("找到 %zu 个潜在 hook\n", hooks.size());
```

## 重要说明

### ⚠️ 安全注意事项

1. **内存权限**: 确保搜索范围可读，或使用 SEH/VEH 异常处理
   ```cpp
   __try {
       SearchPatternSSE2(...);
   } __except(EXCEPTION_EXECUTE_HANDLER) {
       printf("访问违例\n");
   }
   ```

2. **地址验证**: 始终在解引用前检查返回值
   ```cpp
   void* result = nullptr;
   if (!SearchPatternSSE2(..., result) && result != nullptr) {
       // 安全使用 result
   }
   ```

3. **对齐**: 函数内部处理未对齐读取（无需操作）

### 🔧 编译要求

- **C++ 标准**: C++20 或更高（`/std:c++20` 或 `-std=c++20`）
- **MSVC**: 
  - SSE2: `/arch:SSE2`（x64 上默认）
  - AVX2: `/arch:AVX2`
- **GCC/Clang**:
  - SSE2: `-msse2`（x64 上默认）
  - AVX2: `-mavx2`
- **优化**: 推荐 `/O2` 或 `-O2`

### 📊 预取调优指南

| 模式 | 推荐 `prefetch_size` |
|---------|----------------------------|
| 小模式（<16 字节） | 64 |
| 中等模式（16-64 字节） | 128（默认） |
| 大模式（>64 字节） | 256 |
| 数据在缓存中 | 64 |
| 冷数据（很少访问） | 256 |

### 🐛 故障排除

| 问题 | 原因 | 解决方案 |
|-------|-------|----------|
| 总是返回 `true` | 模式从不匹配 | 检查模式语法和内存范围 |
| 调用时崩溃 | 无效内存范围 | 传递前验证指针 |
| 结果错误 | 不正确的偏移/大小 | 用十六进制编辑器/调试器验证 |
| 性能慢 | 错误的 SIMD 版本 | 在现代 CPU 上使用 AVX2 |
| 编译错误 | 缺少 C++20 | 添加 `/std:c++20` 或 `-std=c++20` |

## 高级主题

### 自定义预取策略

```cpp
// 对于可预测访问的紧密循环
const size_t custom_prefetch = 512;
SearchPatternSSE2(..., address, custom_prefetch);
```

### 模块边界处的模式

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

### 多线程扫描

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
    
    // 合并结果
    std::vector<void*> all_results;
    for (const auto& r : results) {
        all_results.insert(all_results.end(), r.begin(), r.end());
    }
    return all_results;
}
```

## 贡献

欢迎贡献！请确保：
- 代码遵循现有风格
- 所有函数保持 API 兼容性
- 优化包含性能基准测试

## 另请参阅

- [English Documentation](README.md)
- [示例代码](example_main.cpp)
