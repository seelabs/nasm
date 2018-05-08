This is a project to lean assembly programming by building encoders and
decoders. The first codecs are for hex and is about 10x faster than the
reference C++ implementation, however it is meant only for learning purposes and
is not well tested.

The file `src/decode.asm` contains the assembly source for `int decode_hex256(char* in, char* out);` 
It decodes a 64 byte hex value into a binary values. 
The file `src/encode.asm` contains the assembly source for `void encode_hex256(char* in, char* out);` 
It encodes a 32 byte binary value into a 64 byte hex value.
The file `src/main.cpp` contains a test driver and benchmark.
The nasm assmebler and the linux calling convention was used.

Why learn assmebly language programming when optimizing compilers are so good?
One reason is the SIMD instructions in AVX. With AVX, instructions operate on
256-bit vectors and can execute on 32-bytes in parallel. To take advantage of
this parallelism often requires different algorithm rather than a clever set of
optimizations.

Consider the hex decoding algorithm to decode a 32 byte value from rippled. The
code is straight-forward: it take every hex digit in pairs, looks up the value
of each digit, and shifts one of them left 4-bits and ors them together. Very
straight forward. In fact, this implementation isn't limited to 256 bit values,
it can handle any number of bytes.

An AVX 32 byte hex encoding algorithm is more compilicated due to the
constraints in the AVX programming model and also because coding in assembly
rather than a high level language. There will be 64 hex digits in a 32 byte
value. The first 32 bytes are put into one register (ymm0), and the second into
another (ymm1). Instead of using a lookup table to convert the hex digit to a
value, the digits are converted through the following algorithm:

1) Check if the digit is 0-9, if so, subtract 48. 
2) If it's not a digit, assmue is a letter (a-f and A-F), and convert to
   upcase by clearing bit six.
3) Subtract 55 from the converted letters.

The conversion does not happen digit by digit, but 32 digits at a time. This is
done by setting a register with a mask for with 0xff for all the 'letter' ascii
and 0x00 for all the digits for all the 'number' ascii inputs. This mask is used
to load different masks to bitwise and with the input (0xff for the digits,
~0x20 for the letters), and also load different values to subtract (48 for the
numbers, 87 for the letters).

After the input is converted into values (ascii '0' to 0x00 ect), the final
result is foundby shifting the "high order" four bits from one byte into the
correct byte postions and storing them in another register (ymm9 and ymm10), and
adding them to the origional. The result will be every other element containing
the correct result. Then a combination of "lane suffles" (`vinserti128`) and
byte shuffles (`vpshufb`) is used to put the bytes into the correct positon.

A test machine with a Xeon CPU E3-1505M v6 @ 3.00GHz ran the 100 million iterations:

| Algo    | C Ref Imp (ms) | Asm Imp (ms) |
|---------|----------------|--------------|
| Encoder | 1790           | 319          |
| Decoder | 4694           | 561          |

There is also the start of a fast base58 decoder. The current implementation is
limited to decoding 256 bit numbers only. There are three implementations for
comparison:

1) Ripple's base58 decoder
2) A C++ reference implementation of the the fast decoder
3) A C++ implementation that uses AVX to convert to base 58^8

While the AVX implementation for computing the coefficients is about 2.25x
faster than computing coefficients in C++, overall it made little difference
(about 6% speedup overall). However, the pure C++ implementation is about 10x
faster than the reference rippled implementation. The following benchmarks are
based on 1 million iterations.

| Algo        | Rippled Ref Imp (ms) | Proposed Imp (ms) |
|-------------|----------------------|-------------------|
| Decoder B58 | 2199                 | 231               |
