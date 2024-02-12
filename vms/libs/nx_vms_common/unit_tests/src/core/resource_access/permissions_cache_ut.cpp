// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <random>

#include <gtest/gtest.h>

#include <core/resource_access/permissions_cache.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::core::access {
namespace test {

using namespace Qn;
using namespace nx::vms::common::test;

class PermissionsCacheTest: public ContextBasedTest
{
public:
    PermissionsCacheTest():
        ContextBasedTest(nx::core::access::Mode::cached)
    {
    }

protected:
    static constexpr int kRoleSubjectsCount = 200;
    static constexpr int kUserSubjectsCount = 1000;
    static constexpr int kOverallSubjectsCount = kRoleSubjectsCount + kUserSubjectsCount;
    static constexpr int kResourcesCount = 12000;

    static void SetUpTestCase()
    {
        // Generate predefined set of Access Subjects Ids.
        std::generate_n(std::back_inserter(subjectIdPool),
            kOverallSubjectsCount, nx::Uuid::createUuid);

        // Generate predefined set of resource Ids.
        std::generate_n(std::back_inserter(resourceIdPool),
            kResourcesCount - kUserSubjectsCount, nx::Uuid::createUuid);

        // Where some Ids are supposed to be User resource Ids, i.e are Access Subjects too.
        std::copy_n(subjectIdPool.cbegin(), kUserSubjectsCount,
            std::back_inserter(resourceIdPool));

        // Heterogeneous order is natural.
        std::random_device randomDevice;
        std::default_random_engine randomEngine(randomDevice());
        std::shuffle(subjectIdPool.begin(), subjectIdPool.end(), randomEngine);
        std::shuffle(resourceIdPool.begin(), resourceIdPool.end(), randomEngine);
    }

    static void TearDownTestCase()
    {
        subjectIdPool.resize(0);
        resourceIdPool.resize(0);
    }

    static std::vector<nx::Uuid> subjectIdPool;
    static std::vector<nx::Uuid> resourceIdPool;
};

std::vector<nx::Uuid> PermissionsCacheTest::subjectIdPool = {};
std::vector<nx::Uuid> PermissionsCacheTest::resourceIdPool = {};

TEST_F(PermissionsCacheTest, fillPerItemSubjectFirst)
{
    PermissionsCache cache;

    for (int s = 0; s < kOverallSubjectsCount; ++s)
    {
        for (int r = 0; r < kResourcesCount; ++r)
            cache.setPermissions(subjectIdPool.at(s), resourceIdPool.at(r), Qn::SavePermission);
    }
}

TEST_F(PermissionsCacheTest, fillPerItemResourcesFirst)
{
    PermissionsCache cache;

    for (int r = 0; r < kResourcesCount; ++r)
    {
        for (int s = 0; s < kOverallSubjectsCount; ++s)
            cache.setPermissions(subjectIdPool.at(s), resourceIdPool.at(r), Qn::SavePermission);
    }
}

TEST_F(PermissionsCacheTest, fillPerItemThanNullifyPerItem)
{
    PermissionsCache cache;

    for (int s = 0; s < kOverallSubjectsCount; ++s)
    {
        for (int r = 0; r < kResourcesCount; ++r)
            cache.setPermissions(subjectIdPool.at(s), resourceIdPool.at(r), Qn::SavePermission);
    }

    for (int s = 0; s < kOverallSubjectsCount; ++s)
    {
        for (int r = 0; r < kResourcesCount; ++r)
            cache.removePermissions(subjectIdPool.at(s), resourceIdPool.at(r));
    }
}

TEST_F(PermissionsCacheTest, fillPerItemRemoveByResource)
{
    PermissionsCache cache;

    for (int s = 0; s < kOverallSubjectsCount; ++s)
    {
        for (int r = 0; r < kResourcesCount; ++r)
            cache.setPermissions(subjectIdPool.at(s), resourceIdPool.at(r), Qn::SavePermission);
    }

    for (int r = 0; r < kResourcesCount; ++r)
        cache.removeResource(resourceIdPool.at(r));
}

TEST_F(PermissionsCacheTest, naiveFillingSubjectRemovingPerformance)
{
    PermissionsCache cache;

    for (int s = 0; s < kOverallSubjectsCount; ++s)
    {
        for (int r = 0; r < kResourcesCount; ++r)
            cache.setPermissions(subjectIdPool.at(s), resourceIdPool.at(r), Qn::SavePermission);
    }

    for (int s = 0; s < kOverallSubjectsCount; ++s)
        cache.removeSubject(resourceIdPool.at(s));
}

TEST_F(PermissionsCacheTest, returnsNullValueByDefault)
{
    PermissionsCache cache;

    const auto subjectId = nx::Uuid::createUuid();
    const auto resourceId = nx::Uuid::createUuid();

    ASSERT_FALSE(bool(cache.permissions(subjectId, resourceId)));
}

TEST_F(PermissionsCacheTest, returnsValueThatWasStored)
{
    PermissionsCache cache;

    const auto subjectId = nx::Uuid::createUuid();
    const auto resourceId = nx::Uuid::createUuid();

    Permissions permissions = {Permission::RemovePermission, Permission::ViewContentPermission};
    cache.setPermissions(subjectId, resourceId, permissions);

    ASSERT_EQ(cache.permissions(subjectId, resourceId), permissions);
}

TEST_F(PermissionsCacheTest, returnsTrueIfValueModifiedFalseIfNot)
{
    PermissionsCache cache;

    const auto subjectId = nx::Uuid::createUuid();
    const auto resourceId = nx::Uuid::createUuid();

    const Permissions permissions =
        {Permission::RemovePermission, Permission::ViewContentPermission};
    const Permissions newPermissions =
        {Permission::RemovePermission, Permission::WriteAccessRightsPermission};

    // Store new record, setter returns true.
    ASSERT_TRUE(cache.setPermissions(subjectId, resourceId, permissions));

    // Store same record again, setter returns false.
    ASSERT_FALSE(cache.setPermissions(subjectId, resourceId, permissions));

    // Overwrite record with new value, setter returns true.
    ASSERT_TRUE(cache.setPermissions(subjectId, resourceId, newPermissions));

    // But only for the first time, just like this.
    ASSERT_FALSE(cache.setPermissions(subjectId, resourceId, newPermissions));
}

TEST_F(PermissionsCacheTest, returnsTrueOnlyIfSomethingWasRemoved)
{
    PermissionsCache cache;

    const auto subjectId = nx::Uuid::createUuid();
    const auto resourceId = nx::Uuid::createUuid();

    const Permissions permissions =
        {Permission::RemovePermission, Permission::ViewContentPermission};

    // Trying remove something never stored: false returned.
    ASSERT_FALSE(cache.removePermissions(subjectId, resourceId));

    // Removing filled record: true returned.
    cache.setPermissions(subjectId, resourceId, permissions);
    ASSERT_TRUE(cache.removePermissions(subjectId, resourceId));
}

TEST_F(PermissionsCacheTest, noPermissionsIsNotNullValue)
{
    PermissionsCache cache;

    const auto subjectId = nx::Uuid::createUuid();
    const auto resourceId = nx::Uuid::createUuid();

    // Store new with NoPermissions (0) value, setter returns true.
    ASSERT_TRUE(cache.setPermissions(subjectId, resourceId, NoPermissions));

    // Non-null optional value returns, as expected.
    ASSERT_TRUE(bool(cache.permissions(subjectId, resourceId)));

    // That non-null value can be successfully removed.
    ASSERT_TRUE(cache.removePermissions(subjectId, resourceId));

    // Value cannot be get anymore.
    ASSERT_FALSE(bool(cache.permissions(subjectId, resourceId)));
}

TEST_F(PermissionsCacheTest, removingSubjectCausesRemovingCorrespondingUserResource)
{
    // TODO: #vbreus Make full test coverage.
}

TEST_F(PermissionsCacheTest, removingUserResourceCausesRemovingCorrespondingSubject)
{
    // TODO: #vbreus Make full test coverage.
}

TEST_F(PermissionsCacheTest, sparseColumnsTest)
{
    // TODO: #vbreus Make full test coverage.
}

} // namespace test
} // namespace nx::core::access
