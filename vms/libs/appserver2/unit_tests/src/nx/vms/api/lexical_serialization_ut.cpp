// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/string_conversion.h>
#include <transaction/transaction.h>

namespace nx::vms::api::test {

TEST(Lexical, enumSerialization)
{
    ASSERT_EQ("saveUser", nx::reflect::toString(ec2::ApiCommand::saveUser));
    ASSERT_EQ("Regular", nx::reflect::toString(ec2::TransactionType::Regular));
}

TEST(Lexical, enumDeserialization)
{
    ASSERT_EQ(
        ec2::ApiCommand::saveUser,
        nx::reflect::fromString<ec2::ApiCommand::Value>("saveUser"));

    ASSERT_EQ(
        ec2::ApiCommand::saveUser,
        nx::reflect::fromString<ec2::ApiCommand::Value>(
            std::to_string((int) ec2::ApiCommand::saveUser)));

    ASSERT_EQ(
        ec2::TransactionType::Regular,
        nx::reflect::fromString<ec2::TransactionType>("Regular"));

    ASSERT_EQ(
        ec2::TransactionType::Regular,
        nx::reflect::fromString<ec2::TransactionType>("Regular"));
}

} // namespace nx::vms::api::test
