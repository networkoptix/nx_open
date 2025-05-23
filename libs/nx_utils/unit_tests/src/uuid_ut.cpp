// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/uuid.h>

TEST(Uuid, valid_uuid_string)
{
    ASSERT_TRUE(nx::Uuid::isValidUuidString(std::string("fc049201-c4ec-4ea7-81da-2afcc8fbfd3a")));
}

TEST(Uuid, invalid_uuid_string)
{
    ASSERT_FALSE(nx::Uuid::isValidUuidString(std::string("fc049201-c4ec-4ea7-81da-2afcc8fbfd3a@nxtest.com")));
}
