// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/api/data/server_flags.h>

namespace nx::vms::api::test {

TEST(ServerModelFlags, FlagsConversions)
{
    using namespace nx::vms::api;
    std::set<ServerFlags> serverFlagsSet;

    for (const auto modelFlag: nx::reflect::enumeration::allEnumValues<ServerModelFlag>())
    {
        auto serverFlags = nx::vms::api::convertModelToServerFlags(modelFlag);
        EXPECT_TRUE(serverFlagsSet.insert(serverFlags).second);
        auto modelFlags = nx::vms::api::convertServerFlagsToModel(serverFlags);
        EXPECT_EQ(modelFlags, ServerModelFlags(modelFlag));
    }
}

} // namespace nx::vms::api::test
