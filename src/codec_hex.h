#pragma once

extern "C" int
decode_hex256(char const* in, char* out);

extern "C" void
encode_hex256(char const* in, char* out);

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <string>

namespace codec {
namespace hex {

inline
int
char_unhex(char xc)
{
    unsigned char c = static_cast<unsigned char>(xc);
    struct hex_tab
    {
        int hex[256];

        hex_tab()
        {
            std::fill(std::begin(hex), std::end(hex), -1);
            for (int i = 0; i < 10; ++i)
                hex['0' + i] = i;
            for (int i = 0; i < 6; ++i)
            {
                hex['A' + i] = 10 + i;
                hex['a' + i] = 10 + i;
            }
        }
        int operator[](unsigned char c) const
        {
            return hex[c];
        }
    };

    static hex_tab xtab;

    return xtab[c];
}

// Reference hex decoder from rippled
/** Parse a hex string into a base_uint
    The string must contain exactly bytes * 2 hex characters and must not
    have any leading or trailing whitespace.
*/
inline
bool
set_hex_exact(const char* psz, char* out_)
{
    unsigned char* out = reinterpret_cast<unsigned char*>(out_);

    for (int i = 0; i < 32; ++i)
    {
        auto hi = char_unhex(*psz++);
        if (hi == -1)
            return false;

        auto lo = char_unhex(*psz++);
        if (lo == -1)
            return false;

        *out++ = (hi << 4) | lo;
    }

    return true;
}

inline
bool
random_test_decode(int iterations, int bad_digits)
{
    constexpr char const alphabet[23] = "0123456789abcdefABCDEF";
    constexpr int max_bad = 4;
    int num_bad = 0;

    std::mt19937 gen;
    // random index into the alphabet
    std::uniform_int_distribution<> rand_index21(0, 21);
    // random index into the input (for inserting bad chars)
    std::uniform_int_distribution<> rand_index63(0, 63);
    // random char
    std::uniform_int_distribution<> rand255(0, 255);
    char input[64];
    char asm_out[32];
    char c_out[32];
    for (int i = 0; i < iterations; ++i)
    {
        for (int j = 0; j < 64; ++j)
        {
            input[j] = alphabet[rand_index21(gen)];
        }

        for (int b = 0; b < bad_digits; ++b)
        {
            while (1)
            {
                auto const r255 = rand255(gen);
                if ((r255 >= '0' && r255 <= '9') ||
                    (r255 >= 'A' && r255 <= 'F') ||
                    (r255 >= 'a' && r255 <= 'f'))
                    continue;
                // we may repeat indexes, but that's OK for the test
                auto const bad_index = rand_index63(gen);
                input[bad_index] = r255;
                break;
            };
        }

        auto const asm_r = decode_hex256(input, asm_out);
        auto const c_r = set_hex_exact(input, c_out);
        if (asm_r != c_r || (asm_r && memcmp(c_out, asm_out, 32)))
        {
            print_diff(input, asm_r, c_r, asm_out, c_out);
            if (++num_bad == max_bad)
                return false;
        }
    }

    return !num_bad;
}

inline
void
encode_hex256_ref(char const* in, char* out)
{
    for (int i = 0; i < 32; ++i)
    {
        out[2 * i] = "0123456789ABCDEF"[((in[i] & 0xf0) >> 4)];
        out[2 * i + 1] = "0123456789ABCDEF"[((in[i] & 0x0f) >> 0)];
    }
}

inline
bool
random_test_encode(int iterations)
{
    constexpr int max_bad = 1;
    int num_bad = 0;

    std::mt19937 gen;
    // random char
    std::uniform_int_distribution<> rand255(0, 255);
    char input[32];
    char asm_out[64];
    char c_out[64];
    for (int i = 0; i < iterations; ++i)
    {
        for (int j = 0; j < 32; ++j)
        {
            input[j] = rand255(gen);
        }
        encode_hex256(input, asm_out);
        encode_hex256_ref(input, c_out);
        if (memcmp(c_out, asm_out, 64))
        {
            print_diff_encode(input, asm_out, c_out);
            if (++num_bad == max_bad)
                return false;
        }
    }

    return !num_bad;
}

inline
void
benchmark_encode()
{
    using timer = std::chrono::high_resolution_clock;

    char out[64];
    char val[32];
    memset(val, 0xf0, 32);

    int const iters = 100'000'000;

    auto time_diff = [](auto start, auto end) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start);
    };

    {
        auto start = timer::now();
        for (int i = 0; i < iters; ++i)
        {
            encode_hex256(val, out);
        }
        auto end = timer::now();
        std::cout << "Enc Asm: " << time_diff(start, end).count() << '\n';
    }

    {
        auto start = timer::now();
        for (int i = 0; i < iters; ++i)
        {
            encode_hex256_ref(val, out);
        }
        auto end = timer::now();
        std::cout << "  Enc C: " << time_diff(start, end).count() << '\n';
    }
}

inline
void
benchmark_decode()
{
    using timer = std::chrono::high_resolution_clock;

    char out[32];
    char const* val =
        "0123456789abcdef0000111122223333444455556666777788889999aaaabbbb";

    int const iters = 100'000'000;

    auto time_diff = [](auto start, auto end) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start);
    };

    {
        auto start = timer::now();
        for (int i = 0; i < iters; ++i)
        {
            decode_hex256(val, out);
        }
        auto end = timer::now();
        std::cout << "Dec Asm: " << time_diff(start, end).count() << '\n';
    }

    {
        auto start = timer::now();
        for (int i = 0; i < iters; ++i)
        {
            set_hex_exact(val, out);
        }
        auto end = timer::now();
        std::cout << "  Dec C: " << time_diff(start, end).count() << '\n';
    }
}

}  // namespace hex
}  // namespace codec
