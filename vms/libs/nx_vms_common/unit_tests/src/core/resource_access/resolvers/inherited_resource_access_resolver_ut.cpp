// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtTest/QSignalSpy>

#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resolvers/inherited_resource_access_resolver.h>
#include <core/resource_access/resolvers/layout_item_access_resolver.h>
#include <core/resource_access/resolvers/own_resource_access_resolver.h>
#include <core/resource_access/resolvers/videowall_item_access_resolver.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/log/format.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>

#include "private/resource_access_resolver_test_fixture.h"
#include "../test_subject_hierarchy.h"

namespace nx::core::access {
namespace test {

using namespace nx::vms::api;
using namespace nx::vms::common::test;

class InheritedResourceAccessResolverTest:
    public ResourceAccessResolverTestFixture,
    public NameToId
{
public:
    virtual AbstractResourceAccessResolver* createResolver() const override
    {
        const auto ownResolver = new OwnResourceAccessResolver(
            manager.get(), systemContext()->globalPermissionsWatcher(), /*parent*/ manager.get());
        const auto videowallResolver = new VideowallItemAccessResolver(
            ownResolver, resourcePool(), /*parent*/ ownResolver);
        const auto layoutResolver = new LayoutItemAccessResolver(
            videowallResolver, resourcePool(), /*parent*/ videowallResolver);
        return new InheritedResourceAccessResolver(layoutResolver, subjects.get());
    }

    virtual void SetUp() override
    {
        subjects.reset(new TestSubjectHierarchy());
        createTestHierarchy();
        ResourceAccessResolverTestFixture::SetUp();
    }

    virtual void TearDown() override
    {
        ResourceAccessResolverTestFixture::TearDown();
        subjects.reset();
        NameToId::clear();
    }

    void createTestHierarchy()
    {
        //      Group 1
        //        |
        //   +----+----+
        //   |         |
        //   v         v
        // Group 2    Group 3
        //   |         |  |
        //   +----+----+  |
        //        |       |
        //        v       v
        //     User 1   User 2

        subjects->addOrUpdate("Group 1", {});
        subjects->addOrUpdate("Group 2", {"Group 1"});
        subjects->addOrUpdate("Group 3", {"Group 1"});
        subjects->addOrUpdate("User 1", {"Group 2", "Group 3"});
        subjects->addOrUpdate("User 2", {"Group 3"});
    }

    QString mapToString(const ResourceAccessMap& map) const
    {
        QStringList lines;
        for (const auto& [id, accessRights]: nx::utils::constKeyValueRange(map))
            lines << nx::format("%1: %2", name(id), nx::reflect::toString(accessRights));

        lines.sort();
        return lines.join(", ");
    }

    QString detailsToString(const ResourceAccessDetails& details) const
    {
        QStringList lines;
        for (const auto& [subjectId, resources]: nx::utils::constKeyValueRange(details))
        {
            QStringList names;
            for (const auto& resource: resources)
                names.push_back(resource ? resource->getName() : "null");

            names.sort();
            lines << nx::format("%1: %2", subjects->name(subjectId), names.join(", "));
        }

        lines.sort();
        return lines.join(", ");
    }

    std::unique_ptr<TestSubjectHierarchy> subjects;
};

TEST_F(InheritedResourceAccessResolverTest, inheritedAccessRights)
{
    const AccessRights testAccessRights{
        AccessRight::view | AccessRight::viewArchive};

    manager->setOwnResourceAccessMap(subjects->id("Group 1"),
        {{ensureId("Camera"), testAccessRights}});

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("Group 2"))),
        mapToString({{id("Camera"), testAccessRights}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 1"))),
        mapToString({{id("Camera"), testAccessRights}}));
}

TEST_F(InheritedResourceAccessResolverTest, mergedAccessRights)
{
    //                                    Group 1 (Camera 1: testAccessRights1)
    //                                       |
    //                                  +----+----+
    //                                  |         |
    //                                  v         v
    //  (Camera 1: testAccessRights2) Group 2    Group 3 (Camera 2: testAccessRights3)
    //                                  |         |  |
    //                                  +----+----+  |
    //                                       |       |
    //                                       v       v
    //                                    User 1   User 2 (Camera 2: testAccessRights4)

    // For proper testing, each of the four permission sets should have an unique permission.

    const AccessRights testAccessRights1{
        AccessRight::view | AccessRight::viewArchive};

    const AccessRights testAccessRights2{
        AccessRight::view | AccessRight::userInput};

    const AccessRights testAccessRights3{
        AccessRight::view | AccessRight::edit};

    const AccessRights testAccessRights4{
        AccessRight::view | AccessRight::viewBookmarks};

    manager->setOwnResourceAccessMap(subjects->id("Group 1"),
        {{ensureId("Camera 1"), testAccessRights1}});

    manager->setOwnResourceAccessMap(subjects->id("Group 2"),
        {{ensureId("Camera 1"), testAccessRights2}});

    manager->setOwnResourceAccessMap(subjects->id("Group 3"),
        {{ensureId("Camera 2"), testAccessRights3}});

    manager->setOwnResourceAccessMap(subjects->id("User 2"),
        {{ensureId("Camera 2"), testAccessRights4}});

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("Group 2"))),
        mapToString({{id("Camera 1"), testAccessRights1 | testAccessRights2}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("Group 3"))),
        mapToString({
            {id("Camera 1"), testAccessRights1},
            {id("Camera 2"), testAccessRights3}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 1"))),
        mapToString({
            {id("Camera 1"), testAccessRights1 | testAccessRights2},
            {id("Camera 2"), testAccessRights3}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 2"))),
        mapToString({
            {id("Camera 1"), testAccessRights1},
            {id("Camera 2"), testAccessRights3 | testAccessRights4}}));
}

TEST_F(InheritedResourceAccessResolverTest, notificationSignals)
{
    const AccessRights testAccessRights{AccessRight::view};

    using Notifier = AbstractResourceAccessResolver::Notifier;
    resolver->notifier()->subscribeSubjects(subjects->ids({
        "Group 1", "Group 2", "Group 3", "User 1"})); //< "User 2" is not included intentionally.

    qRegisterMetaType<UuidSet>();
    QSignalSpy spy(resolver->notifier(), &Notifier::resourceAccessChanged);

    const auto lastNames =
        [&]() -> Names
        {
            if (spy.size() != 1)
                return {};
            const auto args = spy.takeLast();
            if (args.size() != 1)
                return {};
            return subjects->names(args[0].value<UuidSet>());
        };

    manager->setOwnResourceAccessMap(subjects->id("Group 1"),
        {{ensureId("Camera"), testAccessRights}});
    ASSERT_EQ(lastNames(),  Names({"Group 1", "Group 2", "Group 3", "User 1" /*"User 2"*/}));

    // SubjectHierarchy removals aren't automatically translated to AccessRightsManager removals.
    // Therefore we won't receive change notifications for removed subjects themselves.

    subjects->remove("Group 1");
    ASSERT_EQ(lastNames(), Names({"Group 2", "Group 3", "User 1" /*"User 2"*/}));

    subjects->remove("Group 3");
    ASSERT_EQ(lastNames(), Names({"User 1" /*"User 2"*/}));

    subjects->remove("Group 2");
    ASSERT_EQ(lastNames(), Names({"User 1"}));

    subjects->remove("User 1");
    ASSERT_TRUE(spy.empty());
}

TEST_F(InheritedResourceAccessResolverTest, dynamicPermissionChanges)
// This test is intended to check correct cache invalidation when inherited access rights change.
{
    const AccessRights testAccessRights1{
        AccessRight::view | AccessRight::viewArchive};

    const AccessRights testAccessRights2{
        AccessRight::view | AccessRight::userInput};

    const AccessRights testAccessRights3{
        AccessRight::view | AccessRight::edit};

    //      Group 1
    //        |
    //   +----+----+
    //   |         |
    //   v         v
    // Group 2    Group 3
    //   |         |  |
    //   +----+----+  |
    //        |       |
    //        v       v
    //     User 1   User 2

    manager->setOwnResourceAccessMap(subjects->id("User 1"),
        {{ensureId("Camera"), testAccessRights1}});

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 1"))),
        mapToString({{id("Camera"), testAccessRights1}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 2"))), QString());

    manager->setOwnResourceAccessMap(subjects->id("Group 2"),
        {{ensureId("Camera"), testAccessRights2}});

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 1"))),
        mapToString({{id("Camera"), testAccessRights1 | testAccessRights2}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 2"))), QString());

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("Group 2"))),
        mapToString({{id("Camera"), testAccessRights2}}));

    manager->setOwnResourceAccessMap(subjects->id("Group 1"),
        {{ensureId("Camera"), testAccessRights3}});

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 1"))),
        mapToString({{id("Camera"), testAccessRights1 | testAccessRights2 | testAccessRights3}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 2"))),
        mapToString({{id("Camera"), testAccessRights3}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("Group 2"))),
        mapToString({{id("Camera"), testAccessRights2 | testAccessRights3}}));

    subjects->remove("Group 1");

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("Group 2"))),
        mapToString({{id("Camera"), testAccessRights2}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 1"))),
        mapToString({{id("Camera"), testAccessRights1 | testAccessRights2}}));

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 2"))), QString());

    manager->setOwnResourceAccessMap(subjects->id("Group 2"), {});

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("Group 2"))), QString());

    ASSERT_EQ(mapToString(resolver->resourceAccessMap(subjects->id("User 1"))),
        mapToString({{id("Camera"), testAccessRights1}}));
}

TEST_F(InheritedResourceAccessResolverTest, resourceAccessDetails)
{
    //                           Group 1 (Camera 1: view & exportArchive; ALL: view & viewArchive)
    //                                       |
    //                                  +----+----+
    //                                  |         |
    //                                  v         v
    //  (Camera 1: view & userInput) Group 2    Group 3 (Camera 2: view & edit)
    //                                  |         |  |
    //                                  +----+----+  |
    //                                       |       |
    //                                       v       v
    //                                    User 1   User 2 (Camera 2: view & viewBookmarks)

    auto camera1 = addCamera();
    camera1->setName("Camera 1");

    auto camera2 = addCamera();
    camera2->setName("Camera 2");

    manager->setOwnResourceAccessMap(subjects->id("Group 1"),
        {{camera1->getId(), AccessRight::view | AccessRight::viewArchive},
        {kAllDevicesGroupId, AccessRight::view | AccessRight::exportArchive}});

    manager->setOwnResourceAccessMap(subjects->id("Group 2"),
        {{camera1->getId(), AccessRight::view | AccessRight::userInput}});

    manager->setOwnResourceAccessMap(subjects->id("Group 3"),
        {{camera2->getId(), AccessRight::view | AccessRight::edit}});

    manager->setOwnResourceAccessMap(subjects->id("User 2"),
        {{camera2->getId(), AccessRight::view | AccessRight::viewBookmarks}});

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("User 1"), camera1, AccessRight::view)),
        detailsToString({
            {subjects->id("Group 1"), {camera1}},
            {subjects->id("Group 2"), {camera1}}}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("User 1"), camera1, AccessRight::userInput)),
        detailsToString({{subjects->id("Group 2"), {camera1}}}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("User 1"), camera1, AccessRight::edit)),
        detailsToString({}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("User 1"), camera2, AccessRight::view)),
        detailsToString({
            {subjects->id("Group 1"), {camera2}},
            {subjects->id("Group 3"), {camera2}}}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("User 2"), camera1, AccessRight::view)),
        detailsToString({{subjects->id("Group 1"), {camera1}}}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("User 2"), camera2, AccessRight::view)),
        detailsToString({
            {subjects->id("User 2"), {camera2}},
            {subjects->id("Group 1"), {camera2}},
            {subjects->id("Group 3"), {camera2}}}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("User 2"), camera2, AccessRight::viewBookmarks)),
        detailsToString({{subjects->id("User 2"), {camera2}}}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("Group 2"), camera1, AccessRight::view)),
        detailsToString({
            {subjects->id("Group 1"), {camera1}},
            {subjects->id("Group 2"), {camera1}}}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("Group 2"), camera1, AccessRight::userInput)),
        detailsToString({{subjects->id("Group 2"), {camera1}}}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("Group 2"), camera1, AccessRight::edit)),
        detailsToString({}));

    ASSERT_EQ(detailsToString(
        resolver->accessDetails(subjects->id("Group 2"), camera2, AccessRight::exportArchive)),
        detailsToString({{subjects->id("Group 1"), {camera2}}}));
}

TEST_F(InheritedResourceAccessResolverTest, noInheritedDesktopCameraAccess)
{
    auto user = addUser(kViewersGroupId);
    auto camera = addDesktopCamera(user);

    manager->setOwnResourceAccessMap(subjects->id("Group 1"),
        {{camera->getId(), AccessRight::view}});

    ASSERT_EQ(resolver->accessRights(subjects->id("Group 1"), camera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 2"), camera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 3"), camera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("User 1"), camera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("User 2"), camera), AccessRights());
}

TEST_F(InheritedResourceAccessResolverTest, inheritedDesktopCameraAccessViaVideowall)
{
    auto user = addUser(kViewersGroupId);
    auto camera = addDesktopCamera(user);
    auto videowall = addVideoWall();
    auto layout = addLayoutForVideoWall(videowall);
    addToLayout(layout, camera);

    manager->setOwnResourceAccessMap(subjects->id("Group 1"),
        {{videowall->getId(), AccessRight::edit}});

    const AccessRights expectedRights = AccessRight::view;

    ASSERT_EQ(resolver->accessRights(subjects->id("Group 1"), camera), expectedRights);
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 2"), camera), expectedRights);
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 3"), camera), expectedRights);
    ASSERT_EQ(resolver->accessRights(subjects->id("User 1"), camera), expectedRights);
    ASSERT_EQ(resolver->accessRights(subjects->id("User 2"), camera), expectedRights);
}

TEST_F(InheritedResourceAccessResolverTest, inheritedAdminAccessRights)
{
    auto group = createUserGroup("Power User Group", kPowerUsersGroupId);
    subjects->setId("Power User Group", group.id);
    subjects->addOrUpdate(group.id, {kPowerUsersGroupId});
    subjects->addOrUpdate("Group 2", {"Group 1", "Power User Group"});

    ASSERT_FALSE(resolver->hasFullAccessRights(subjects->id("Group 1")));
    ASSERT_TRUE(resolver->hasFullAccessRights(subjects->id("Power User Group")));
    ASSERT_TRUE(resolver->hasFullAccessRights(subjects->id("Group 2")));
    ASSERT_FALSE(resolver->hasFullAccessRights(subjects->id("Group 3")));
    ASSERT_TRUE(resolver->hasFullAccessRights(subjects->id("User 1")));
    ASSERT_FALSE(resolver->hasFullAccessRights(subjects->id("User 2")));

    auto camera = addCamera();
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 1"), camera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("Power User Group"), camera), kFullAccessRights);
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 2"), camera), kFullAccessRights);
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 3"), camera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("User 1"), camera), kFullAccessRights);
    ASSERT_EQ(resolver->accessRights(subjects->id("User 2"), camera), AccessRights());

    auto user = addUser(kPowerUsersGroupId);
    auto desktopCamera = addDesktopCamera(user);
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 1"), desktopCamera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("Power User Group"), desktopCamera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 2"), desktopCamera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("Group 3"), desktopCamera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("User 1"), desktopCamera), AccessRights());
    ASSERT_EQ(resolver->accessRights(subjects->id("User 2"), desktopCamera), AccessRights());
}

} // namespace test
} // namespace nx::core::access
