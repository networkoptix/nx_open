// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource_management/resource_pool.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/vms/api/data/user_role_data.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::vms::common::test {

class QnCachedPermissionsPerformanceTest: public ContextBasedTest
{
public:
    QnCachedPermissionsPerformanceTest():
        ContextBasedTest(nx::core::access::Mode::cached)
    {
    }

    template<typename Action>
    void run(const Action& action)
    {
        NX_INFO(this, "BEGIN ---------------");
        const auto count = nx::utils::TestOptions::applyLoadMode<size_t>(10);
        for (size_t i = 0; i < count; ++i)
            action();
        NX_INFO(this, "END ---------------");
    }
};

TEST_F(QnCachedPermissionsPerformanceTest, createUsers)
{
    run([&] { addUser(GlobalPermission::none); } );
}

TEST_F(QnCachedPermissionsPerformanceTest, createUsersInRole)
{
    const auto r = createRole(GlobalPermission::none);
    run(
        [&]
        {
            const auto u = addUser(GlobalPermission::none);
            u->setUserRoleIds({r.id});
        });
}

TEST_F(QnCachedPermissionsPerformanceTest, createUsersInRoles)
{
    run(
        [&]
        {
            const auto r = createRole(GlobalPermission::none);
            const auto u = addUser(GlobalPermission::none);
            u->setUserRoleIds({r.id});
        });
}

} // namespace nx::vms::common::test
