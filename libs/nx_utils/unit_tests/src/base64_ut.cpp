// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/base64.h>

namespace nx::utils {

TEST(Base64, decoded_length_estimate)
{
    for (int len = 1; len < 16; ++len)
    {
        std::string s(len, 'x');
        std::generate(s.begin(), s.end(), &rand);
        const auto encoded = toBase64(s);
        ASSERT_EQ((int) s.size(), fromBase64(encoded.data(), encoded.size(), nullptr, 0));
    }
}

} // namespace nx::utils
