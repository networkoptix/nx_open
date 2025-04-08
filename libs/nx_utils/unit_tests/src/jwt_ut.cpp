// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/jwt.h>

TEST(Jwt, token_trim)
{
    std::string validJwt =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
        ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ";
    std::string signature = "SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c";
    ASSERT_EQ(validJwt, nx::utils::jwtWithoutSignature(validJwt + "." + signature));
    ASSERT_EQ("", nx::utils::jwtWithoutSignature(signature));
}
