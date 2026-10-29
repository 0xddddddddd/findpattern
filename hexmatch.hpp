#ifndef __HEXMATCH_H__
#define __HEXMATCH_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#include <iostream>
#include <string>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <bit>
#include <immintrin.h>

#ifndef CastSet
#   define CastSet(t, x) *std::bit_cast<t*>(x)
#endif

#ifndef CastGet
#   define CastGet(t, x) *std::bit_cast<t*>(x)
#endif

#ifndef CastBit
#   define CastBit(t, x) std::bit_cast<t>(x)
#endif

#ifndef CastRef
#   define CastRef(t, x) (*std::bit_cast<t*>(std::bit_cast<std::uintptr_t>(x)))
#endif

#if defined(DEBUG) || defined(_DEBUG)
#   pragma optimize("gt", on)
#endif

enum math_type
{
    math_constant,
    math_imm,
    math_address
};

static constexpr char charhexset[] = "0123456789ABCDEFabcdef";

static bool StringReplace(std::string& source, const std::string& delimiters, const std::string& dispose = "", const std::size_t offset = 1)
{
    if (source.empty()) {
        return false;
    }

    if (delimiters.empty()) {
        return true;
    }

    for (std::string::size_type pos = source.find(delimiters); pos != std::string::npos; pos = source.find(delimiters)) {
        if (source.replace(pos, offset, dispose).size()) {
            continue;
        }
        return false;
    }

    return true;
}

static bool Hexadec2xdigit(const std::string& data, std::string& buffer, std::size_t offset)
{
    if (data.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < data.size(); i += offset) {
        if (isxdigit(data[i])) {
            buffer += static_cast<char>(std::stoul(data.substr(i, offset), nullptr, 16));
        } else {
            buffer += 'X';
        }
    }

    return true;
}


static bool Hexadec2xdigitEx(const std::string& data, std::string& buffer, std::uint32_t checkxdigit = 0, std::uint32_t transform = 1)
{
    if (data.empty()) {
        return false;
    }

    std::string masks(data);
    {
        // 去掉 空格
        if (StringReplace(masks, " ")) {
            // 大小写转换
            if (masks.end() == (1 == transform ? std::transform(masks.begin(), masks.end(), masks.begin(), [] (unsigned char ch) { return toupper(ch); }) : std::transform(masks.begin(), masks.end(), masks.begin(), [] (unsigned char ch) { return tolower(ch); }))) {
                // 检查是否是完整的十六进制数
                if (checkxdigit) {
                    if (std::string::npos != masks.find_first_not_of(charhexset)) {
                        return false;
                    }
                }

                return Hexadec2xdigit(static_cast<const std::string&>(masks), buffer, 2);
            }
        }
    }

    return false;
}

static bool SearchPattern(const std::uint8_t* pos, const std::string_view& chars)
{
    for (std::string_view::const_iterator xdigit = chars.cbegin(); xdigit != chars.cend(); ++xdigit, ++pos) {
        if (*pos != static_cast<const std::uint8_t>(*xdigit) &&           // no match
            'X' != static_cast<const std::uint8_t>(*xdigit))              // filter out arbitrary characters
        {
            return false;
        }
        // else hit character
    }

    return true;
}

static bool SearchPattern(const std::uint8_t* start,                      // start address
                          const std::uint8_t* end,                        // end address
                          const std::string& keyword,                     // characteristic code
                          std::size_t index,                              // take out the serial number
                          std::size_t offset,                             // pointer offset
                          std::size_t size,
                          math_type type,						          // is call
                          void*& address)                                 // return to address
{
    if (keyword.empty()) {
        return true;
    }

    if (start != end && end > start && static_cast<std::size_t>(end - start) > keyword.size()) {
        std::string chars;
        {
            if (Hexadec2xdigitEx(keyword, chars, 0)) {
                for (auto [pos, i] = std::make_tuple(start, static_cast<std::size_t>(0)); pos != end; ++pos) {
                    if (SearchPattern(pos, chars)) {
                        if (++i != index) {
                            continue;
                        }

                        switch (type) {
                            case math_constant:
                                switch (size) {
                                    case 1: address = reinterpret_cast<void*>(CastGet(std::uint8_t, reinterpret_cast<std::intptr_t>(pos) + offset));  break;
                                    case 2: address = reinterpret_cast<void*>(CastGet(std::uint16_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                                    case 4: address = reinterpret_cast<void*>(CastGet(std::uint32_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                                    case 8: address = reinterpret_cast<void*>(CastGet(std::uint64_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                                    default: break;
                                }
                                break;
                            case math_imm:
                                address = reinterpret_cast<void*>(reinterpret_cast<std::intptr_t>(CastGet(void*, reinterpret_cast<std::intptr_t>(pos) + offset)) + reinterpret_cast<std::intptr_t>(pos + (offset - 1) + 5));
                                break;
                            case math_address:
                                address = reinterpret_cast<void**>(reinterpret_cast<std::intptr_t>(pos) + offset);
                                break;
                            default: break;
                        }

                        return false;
                    }
                }
            }
        }
    }

    return true;
}

template <typename Type = void*>
static bool SearchPattern(const std::intptr_t start,                     // start address
                          const std::intptr_t end,                        // end address
                          const std::string& keyword,                     // characteristic code
                          std::size_t index,                              // take out the serial number
                          std::size_t offset,                             // pointer offset
                          std::size_t size,
                          math_type type,						          // is call
                          Type& address)                                  // return to address
{
    return SearchPattern(reinterpret_cast<const std::uint8_t*>(start), reinterpret_cast<const std::uint8_t*>(end), keyword, index, offset, size, type, reinterpret_cast<void*&>(address));
}

static bool SearchPatternSSE2(const std::uint8_t* start,                    // start address
                              const std::uint8_t* end,                      // end address
                              const std::string& chars,                     // characteristic code
                              std::size_t index,                            // take out the serial number
                              std::size_t offset,                           // pointer offset
                              void*& address,                               // return to address
                              std::size_t size,
                              math_type type,						        // is call
                              std::size_t count,                            // number of characteristic multiples
                              std::size_t prefetch_size = 128

)
{
    for (auto [masks, i, n, k, pos] = std::make_tuple(std::array<std::size_t, 32>{}, static_cast<std::size_t>(0), static_cast<std::size_t>(0), static_cast<std::size_t>(0), start); i < count; ++i) {
        for (std::size_t j = 0; j < 16 && n < chars.size(); ++j, ++n) {
            if ('X' != reinterpret_cast<const std::int8_t*>(chars.c_str() + i * 16)[j]) {
                masks[i] |= static_cast<std::size_t>(1) << j;
            }
        }

        for (auto [pattern, j] = std::make_tuple(_mm_loadu_si128(reinterpret_cast<const __m128i*>(chars.c_str() + i * 16)), static_cast<std::size_t>(0));
             pos <= end;
             ++pos) {
            if (pos + prefetch_size <= end) {
                // _MM_HINT_NTA   Non-Temporal Access (不污染缓存)
                // _MM_HINT_T0    加载到 L1/L2/L3
                // _MM_HINT_T1    加载到 L2/L3（跳过 L1）
                // _MM_HINT_T2    加载到 L3（跳过 L1/L2）
                _mm_prefetch(reinterpret_cast<const char*>(pos + prefetch_size), _MM_HINT_NTA);
            }
            // block check
            if (pos + (i * 16) + 16 > end) {
                break;
            }

            if (static_cast<std::size_t>(static_cast<std::size_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pos + (i * 16))), pattern))) & masks[i]) == masks[i]) {
                if (n < chars.size()) {
                    break;
                }

                if (++k != index) {
                    continue;
                }

                switch (type) {
                    case math_constant:
                        switch (size) {
                            case 1: address = reinterpret_cast<void*>(CastGet(std::uint8_t, reinterpret_cast<std::intptr_t>(pos) + offset));  break;
                            case 2: address = reinterpret_cast<void*>(CastGet(std::uint16_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                            case 4: address = reinterpret_cast<void*>(CastGet(std::uint32_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                            case 8: address = reinterpret_cast<void*>(CastGet(std::uint64_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                            default: break;
                        }
                        break;
                    case math_imm:
                        address = reinterpret_cast<void*>(reinterpret_cast<std::intptr_t>(CastGet(void*, reinterpret_cast<std::intptr_t>(pos) + offset)) + reinterpret_cast<std::intptr_t>(pos + (offset - 1) + 5));
                        break;
                    case math_address:
                        address = reinterpret_cast<void**>(reinterpret_cast<std::intptr_t>(pos) + offset);
                        break;
                    default: break;
                }

                return false;
            }
        }
    }

    return true;
}

static bool SearchPatternSSE2(const std::uint8_t* start,                    // start address
                              const std::uint8_t* end,                      // end address
                              const std::string& keyword,                   // characteristic code
                              std::size_t index,                            // take out the serial number
                              std::size_t offset,                           // pointer offset
                              std::size_t size,
                              math_type type,						          // is call
                              void*& address,                               // return to address
                              std::size_t prefetch_size = 128
)
{
    if (keyword.empty()) {
        return true;
    }

    if (start != end && end > start && static_cast<std::size_t>(end - start) > keyword.size()) {
        std::string chars;
        {
            if (Hexadec2xdigitEx(keyword, chars, 0)) {
                return SearchPatternSSE2(start,
                                         end,
                                         chars,
                                         index,
                                         offset,
                                         address,
                                         size,
                                         type,
                                         static_cast<std::size_t>((((chars.size()) + (16 - 1)) & ~(16 - 1)) >> 4));
            }
        }
    }

    return true;
}

template <typename Type = void*>
static bool SearchPatternSSE2(std::intptr_t start,                          // start address
                              std::intptr_t end,                            // end address
                              const std::string& keyword,                   // characteristic code
                              std::size_t index,                            // take out the serial number
                              std::size_t offset,                           // pointer offset
                              std::size_t size,
                              math_type type,						          // is call
                              Type& address,                                // return to address
                              std::size_t prefetch_size = 128
)
{
    return SearchPatternSSE2(reinterpret_cast<const std::uint8_t*>(start),
                             reinterpret_cast<const std::uint8_t*>(end),
                             keyword,
                             index,
                             offset,
                             size,
                             type,
                             reinterpret_cast<void*&>(address));
}

static bool SearchPatternAVX2(const std::uint8_t* start,                  // start address
                              const std::uint8_t* end,                    // end address
                              const std::string& chars,                   // characteristic code (already processed)
                              std::size_t index,                          // take out the serial number
                              std::size_t offset,                         // pointer offset
                              void*& address,                             // return to address
                              std::size_t size,
                              math_type type,                             // is call
                              std::size_t count,                          // number of 32-byte blocks
                              std::size_t prefetch_size = 128
)
{
    for (auto [masks, i, n, k, pos] = std::make_tuple(std::array<std::uint64_t, 32>{}, static_cast<std::size_t>(0), static_cast<std::size_t>(0), static_cast<std::size_t>(0), start); i < count; ++i) {
        for (std::size_t j = 0; j < 32 && n < chars.size(); ++j, ++n) {
            if ('X' != reinterpret_cast<const std::int8_t*>(chars.c_str() + i * 32)[j]) {
                masks[i] |= (static_cast<std::uint64_t>(1) << j);
            }
        }

        for (auto [pattern, j] = std::make_tuple(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(chars.c_str() + i * 32)), static_cast<std::size_t>(0));
             pos <= end;
             ++pos) {
            if (pos + prefetch_size <= end) {
                _mm_prefetch(reinterpret_cast<const char*>(pos + prefetch_size), _MM_HINT_NTA);
            }

            if (pos + (i * 32) + 32 > end) {
                break;
            }

            if ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(pos + (i * 32))), pattern)))) & masks[i]) == masks[i]) {
                if (n < chars.size()) {
                    break;
                }

                if (++k != index) {
                    continue;
                }

                switch (type) {
                    case math_constant:
                        switch (size) {
                            case 1: address = reinterpret_cast<void*>(CastGet(std::uint8_t, reinterpret_cast<std::intptr_t>(pos) + offset));  break;
                            case 2: address = reinterpret_cast<void*>(CastGet(std::uint16_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                            case 4: address = reinterpret_cast<void*>(CastGet(std::uint32_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                            case 8: address = reinterpret_cast<void*>(CastGet(std::uint64_t, reinterpret_cast<std::intptr_t>(pos) + offset)); break;
                            default: break;
                        }
                        break;
                    case math_imm:
                        address = reinterpret_cast<void*>(reinterpret_cast<std::intptr_t>(CastGet(void*, reinterpret_cast<std::intptr_t>(pos) + offset)) + reinterpret_cast<std::intptr_t>(pos + (offset - 1) + 5));
                        break;
                    case math_address:
                        address = reinterpret_cast<void**>(reinterpret_cast<std::intptr_t>(pos) + offset);
                        break;
                    default: break;
                }

                return false;
            }
        }
    }

    return true;
}

static bool SearchPatternAVX2(const std::uint8_t* start,                  // start address
                              const std::uint8_t* end,                    // end address
                              const std::string& keyword,                 // characteristic code
                              std::size_t index,                          // take out the serial number
                              std::size_t offset,                         // pointer offset
                              std::size_t size,
                              math_type type,                             // is call
                              void*& address,                             // return to address
                              std::size_t prefetch_size = 128
)
{
    if (keyword.empty()) {
        return true;
    }

    if (start != end && end > start && static_cast<std::size_t>(end - start) > keyword.size()) {
        std::string chars;
        {
            if (Hexadec2xdigitEx(keyword, chars, 0)) {
                return SearchPatternAVX2(start,
                                         end,
                                         chars,
                                         index,
                                         offset,
                                         address,
                                         size,
                                         type,
                                         static_cast<std::size_t>((((chars.size()) + (32 - 1)) & ~(32 - 1)) >> 5));
            }
        }
    }

    return true;
}

template <typename Type = void*>
static bool SearchPatternAVX2(std::intptr_t start,                        // start address
                              std::intptr_t end,                          // end address
                              const std::string& keyword,                 // characteristic code
                              std::size_t index,                          // take out the serial number
                              std::size_t offset,                         // pointer offset
                              std::size_t size,
                              math_type type,                             // is call
                              Type& address,                              // return to address
                              std::size_t prefetch_size = 128
)
{
    return SearchPatternAVX2(reinterpret_cast<const std::uint8_t*>(start),
                             reinterpret_cast<const std::uint8_t*>(end),
                             keyword,
                             index,
                             offset,
                             size,
                             type,
                             reinterpret_cast<void*&>(address));
}

#if defined(DEBUG) || defined(_DEBUG)
#   pragma optimize("gt", off)
#endif

#endif // __HEXMATCH_H__