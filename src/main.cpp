#include "codec_base58.h"
#include "codec_hex.h"

int
main()
{
    {
        using namespace codec::base58;
        test_base58();
        return 1;
        benchmark_decode_base58();
        return 1;
        check_base58_8_coeff();
        return 1;
    }

    {
        using namespace codec::hex;
        if (!random_test_encode(1'000'000))
        {
            return 1;
        }
        benchmark_encode();

        if (!random_test_decode(1'000'000, 0) ||
            !random_test_decode(1'000'000, 1))
        {
            return 1;
        }
        benchmark_decode();
    }

    return 0;
}
