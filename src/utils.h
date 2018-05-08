#pragma once

#include <iomanip>
#include <iostream>

void
print_it(char const* in, int num_chars, bool hex)
{
    auto const f = std::cerr.flags();

    if (hex)
    {
        for (int i = 0; i < num_chars; ++i)
            std::cerr << std::setfill('0') << std::setw(2) << std::hex
                      << (int)static_cast<unsigned char>(in[i]) << " ";
    }
    else
    {
        for (int i = 0; i < num_chars/2; ++i)
            std::cerr << in[2*i] << in[2*i+1] << " ";
    }

    std::cerr.flags(f);
};

void
print_diff(
    char const* input,
    bool asm_r,
    bool c_r,
    char const* asm_out,
    char const* c_out)
{
    std::cerr << "Mismatch input on: ";
    print_it(input, 64, false);
    std::cerr << '\n';

    std::cerr << "asm_r: " << asm_r << " c_r: " << c_r << '\n';
    std::cerr << "asm_out: ";
    print_it(asm_out, 32, true);
    std::cerr << '\n';
    std::cerr << "  c_out: ";
    print_it(c_out, 32, true);
    std::cerr << '\n';

    for (int i = 0; i < 32; ++i)
        if (c_out[i] != asm_out[i])
        {
            std::cerr << "First mismatch index: " << i << '\n';
            break;  // only report the first mismatch
        }
}

void
print_diff_encode(
    char const* input,
    char const* asm_out,
    char const* c_out)
{
    std::cerr << "Mismatch\n";
    std::cerr<< "     in: ";
    print_it(input, 32, true);
    std::cerr << '\n';
    std::cerr << "asm_out: ";
    print_it(asm_out, 64, false);
    std::cerr << '\n';
    std::cerr << "  c_out: ";
    print_it(c_out, 64, false);
    std::cerr << '\n';

    for (int i = 0; i < 64; ++i)
        if (c_out[i] != asm_out[i])
        {
            std::cerr << "First mismatch index: " << i << '\n';
            break;  // only report the first mismatch
        }
}
