#pragma once

#include "utils.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <string>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/container/small_vector.hpp>

extern "C" int base58_8_coeff(unsigned char const* in, std::uint64_t* out, unsigned int const* alphabet);

namespace codec {
namespace base58 {

static char rippleAlphabet[] =
    "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz";

// Maps characters to their base58 digit
class InverseAlphabet
{
private:
    std::array<unsigned char, 256> map_;
    // Assembly routines need this as a dword
    std::array<unsigned int, 256> int_map_;

public:
    explicit
    InverseAlphabet(std::string const& digits)
    {
        map_.fill(-1);
        int i = 0;
        for(auto const c : digits)
            map_[static_cast<
                unsigned char>(c)] = i++;
        std::copy(map_.begin(), map_.end(), int_map_.begin());
    }

    unsigned int const* dmap_data() const{
        return int_map_.data();
    };

    int
    operator[](char c) const
    {
        return map_[static_cast<
            unsigned char>(c)];
    }
};

static InverseAlphabet rippleInverse(rippleAlphabet);

/**
   Decode a 256-bit base58 number.

   @note: This show a 10x speed improvement over the reference ripple
   implementation (which is the same as bitcoin). The ripple implementation was
   modified so it does not allocate, to make the benchmark fair.

   The speed improvement comes from first encoding the base 58 number as a base
   58^10 number. In this representation, all the coefficients will fit into a 64
   bit number. This base 58^10 is then decoded into a 256-bit result. This
   reduces the number of multiplications and additions that happen in non-native
   bit lengths.
*/
bool
decode_base58_ref(
    unsigned char const* in,
    int n,
    unsigned char* out,
    InverseAlphabet const& alphabet)
{
    using namespace boost::multiprecision;

    assert(n == 44);

    std::array<std::uint64_t, 5> b5810{};
    {
        // convert from base58 to base 58^10; All values will fit in a 64-bit
        // uint without overflow
        std::array<std::uint64_t, 10> b5810_powers{0x1,
                                                   0x3A,
                                                   0xD24,
                                                   0x2FA28,
                                                   0xACAD10,
                                                   0x271F35A0,
                                                   0x8DD122640,
                                                   0x202161CAA80,
                                                   0x7479027EA100,
                                                   0x1A636A90B07A00};
        int i = 0;
        int b5810i = 0;
        while (1)
        {
            auto const count = std::min(10, n - i);
            std::uint64_t s = 0;
            for (int j = 0; j < count; ++j, ++i)
            {
                auto const val = alphabet[in[n - i - 1]];
                if (val  == 0xff)
                    return false;
                s += b5810_powers[j] * val;  // big endian
            }
            b5810[b5810i] = s;
            if (i >= n)
            {
                assert(i == n);
                break;
            }
            ++b5810i;
        }
    };

    try
    {
        static checked_uint128_t const c58_10{"0x05FA8624C7FBA400"};
        static checked_uint256_t const c58_20{"0x0023BE67B5F0F2889AAF505301100000"};
        static checked_uint256_t const c58_30{
            "0x0000D5B2B2A25E006D5A3847EC548C471CEAA75E40000000"};
        static checked_uint256_t const c58_40{
            "0x000004FD9DF9DBF7E28ED5357BA4A062C19C48E628F6AF73B06121000000000"
            "0"};

        checked_uint128_t const low_result = b5810[0] + c58_10 * b5810[1];
        checked_uint256_t const high_result =
            b5810[2] * c58_20 + b5810[3] * c58_30 + b5810[4] * c58_40;
        checked_uint256_t const result = low_result + high_result;
        assert(result.backend().size() <= 4);
        memset(out, 0, 32);
        export_bits(result, out, 8);
        return true;
    }
    catch (std::overflow_error const&)
    {
        return false;
    }
}

bool
decode_base58_asm(
    unsigned char const* in,
    int n,
    unsigned char* out,
    InverseAlphabet const& alphabet)
{
    using namespace boost::multiprecision;

    assert(n == 44);

    std::array<std::uint64_t, 6> b588{};
    base58_8_coeff(in, &b588[0], alphabet.dmap_data());

    static uint128_t const c58_8{"0x7479027EA100"};
    static uint128_t const c58_16{"0x34FDE3761DA26B26E1410000"};
    static uint256_t const c58_24{"0x181C17963A5226BD66DC1C01D3A7E1000000"};
    static uint256_t const c58_32{"0xAF820335D9B3D9CF58B911D87035677FB7F528100000000"};
    static uint256_t const c58_40{"0x000004FD9DF9DBF7E28ED5357BA4A062C19C48E628F6AF73B061210000000000"};

    uint128_t const low_result = b588[0] + c58_8 * b588[1] + c58_16 * b588[2];
    uint256_t const high_result =
            c58_24 * b588[3] + c58_32 * b588[4] + c58_40 * b588[5];
    uint256_t const result = low_result + high_result;
    assert(result.backend().size() <= 4);
    memset(out, 0, 32);
    export_bits(result, out, 8);
    return true;
}

bool check_base58_8_coeff()
{
    unsigned char const* in = reinterpret_cast<unsigned char const*>(
        "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jk");
    int const n = 44;
    auto const alphabet = rippleInverse;

    assert(n==44);

    std::array<std::uint64_t, 6> const c_coeff = [&] {
        // convert from base58 to base 58^8; All values will fit in a 64-bit
        // uint without overflow
        std::array<std::uint64_t, 6> result{};
        std::array<std::uint64_t, 8> b588_powers{0x1,
                                                 0x3A,
                                                 0xD24,
                                                 0x2FA28,
                                                 0xACAD10,
                                                 0x271F35A0,
                                                 0x8DD122640,
                                                 0x202161CAA80};
        int i = 0;
        int b588i = 0;
        while (1)
        {
            auto const count = std::min(8, n - i);
            std::uint64_t s = 0;
            for (int j = 0; j < count; ++j, ++i)
            {
                auto const val = alphabet[in[n - i - 1]];
                // if (val  == 0xff)
                //     return false;
                s += b588_powers[j] * val;  // big endian
            }
            result[b588i] = s;
            if (i >= n)
            {
                assert(i == n);
                return result;
            }
            ++b588i;
        }
    }();

    std::array<std::uint64_t, 6> asm_coeff;
    base58_8_coeff(in, &asm_coeff[0], alphabet.dmap_data());

    if (c_coeff != asm_coeff)
    {
        auto const f = std::cerr.flags();

        std::cerr << "Mismatch on coeff\n";
        std::cerr << std::setfill('0');
        for (int j = 0; j < 3; ++j)
        {
            std::cerr << " asm " << j << ": ";
            for (int i = j * 2; i < 2*(j + 1); ++i)
            {
                std::cerr << std::setw(20) << asm_coeff[i] << " ";
            }
            std::cerr << "\n";
            std::cerr << "   c " << j << ": ";
            for (int i = j * 2; i < 2 * (j + 1); ++i)
            {
                std::cerr << std::setw(20) << c_coeff[i] << " ";
            }
            std::cerr << "\n\n";
        }

        std::cerr.flags(f);
        return false;
    }

    return true;
}


// Code from Bitcoin: https://github.com/bitcoin/bitcoin
// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Modified from the original
bool
decode_base58_bitcoin (unsigned char const* in,
                       int n,
                       unsigned char* out,
                       InverseAlphabet const& inv)
{
    auto psz = in;
    auto remain = n;
    // Skip and count leading zeroes
    int zeroes = 0;
    while (remain > 0 && inv[*psz] == 0)
    {
        ++zeroes;
        ++psz;
        --remain;
    }
    // Allocate enough space in big-endian base256 representation.
    // log(58) / log(256), rounded up.
    boost::container::small_vector<unsigned char, 32> b256;
    b256.resize(remain * 733 / 1000 + 1);
    while (remain > 0)
    {
        auto carry = inv[*psz];
        if (carry == 255)
            return false;
        // Apply "b256 = b256 * 58 + carry".
        for (auto iter = b256.rbegin();
            iter != b256.rend(); ++iter)
        {
            carry += 58 * *iter;
            *iter = carry % 256;
            carry /= 256;
        }
        assert(carry == 0);
        ++psz;
        --remain;
    }
    // Skip leading zeroes in b256.
    auto iter = std::find_if(
        b256.begin(), b256.end(),[](unsigned char c)
            { return c != 0; });
    // hack: support 256 bit values only
    memset((void*)out, 0, 32);
    while (iter != b256.end())
    {
        *out++ = *iter++;
    }
    return true;
}

void test_base58()
{
    int const iters = 1'000'000;

    unsigned char out_bitcoin[32];
    unsigned char out_cref[32];
    unsigned char val[44];

    std::mt19937 gen;
    std::uniform_int_distribution<> rand_index(0, 57);

    int num_success = 0;
    for (int i = 0; i < iters; ++i)
    {
        for (int j = 0; j < 44; ++j)
            val[j] = rippleAlphabet[rand_index(gen)];

        auto const bcr =
            decode_base58_bitcoin(val, 44, out_bitcoin, rippleInverse);
        auto const rr = decode_base58_ref(val, 44, out_cref, rippleInverse);
        if (!rr)
            continue;
        ++num_success;
        if ((bcr != rr) || (rr && memcmp(out_bitcoin, out_cref, 32)))
        {
            std::cerr << "bitcoin: ";
            print_it(reinterpret_cast<char*>(out_bitcoin), 32, true);
            std::cerr << '\n';
            std::cerr << "   cref: ";
            print_it(reinterpret_cast<char*>(out_cref), 32, true);
            std::cerr << '\n';
            std::cerr << "Failed after: " << i << " iterations.\n";
            return;
        }
    }

    std::cerr << "Checked: " << num_success
              << " Overflows: " << iters - num_success << '\n';
}

void
benchmark_decode_base58()
{
    using timer = std::chrono::high_resolution_clock;


    unsigned char out[32];
    char val[44];
    {
        std::mt19937 gen;
        std::uniform_int_distribution<> rand_index(1, 2);
        for(int i=0;i<44;++i)
            val[i] = rippleAlphabet[rand_index(gen)];
    }

    int const iters = 1'000'000;

    auto time_diff = [](auto start, auto end) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start);
    };

    {
        auto start = timer::now();
        for (int i = 0; i < iters; ++i)
        {
            decode_base58_bitcoin(reinterpret_cast<unsigned char const*>(val), 44, out, rippleInverse);
        }
        auto end = timer::now();
        std::cout << "Dec bitcoin: " << time_diff(start, end).count() << '\n';
    }

    {
        auto start = timer::now();
        for (int i = 0; i < iters; ++i)
        {
            // need if or optimizer will skip call
            if (!decode_base58_ref(reinterpret_cast<unsigned char const*>(val), 44, out, rippleInverse))
                return;
        }
        auto end = timer::now();
        std::cout << "    Dec ref: " << time_diff(start, end).count() << '\n';
    }

    {
        auto start = timer::now();
        for (int i = 0; i < iters; ++i)
        {
            decode_base58_asm(reinterpret_cast<unsigned char const*>(val), 44, out, rippleInverse);
        }
        auto end = timer::now();
        std::cout << "    Dec asm: " << time_diff(start, end).count() << '\n';
    }
}

}
}
