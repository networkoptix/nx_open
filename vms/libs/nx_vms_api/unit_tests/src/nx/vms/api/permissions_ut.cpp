// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json/serializer.h>
#include <nx/vms/api/data/global_permission_deprecated.h>

namespace nx::vms::api::test {

using Old = GlobalPermissionDeprecated;

struct PermissionsV1
{
    GlobalPermissionsDeprecated permissions = Old::none;
    std::optional<std::vector<QnUuid>> accessibleResources;
    bool isOwner = false;
    GlobalPermissions newPermissions{};
};
NX_REFLECTION_INSTRUMENT(PermissionsV1, (permissions)(accessibleResources))

TEST(Permissions, BackwardCompatibility)
{
    const auto resourceId = QnUuid::createUuid();
    const std::vector<PermissionsV1> deprecatedPermissions{
        {Old::customUser | Old::editCameras | Old::accessAllMedia, {}, false, {}},
        {Old::customUser | Old::editCameras, {{resourceId}}, false, {}},
        {Old::customUser | Old::viewArchive | Old::accessAllMedia, {}, false, {}},
        {Old::customUser | Old::viewArchive, {{resourceId}}, false, {}},
        {Old::customUser | Old::exportArchive | Old::accessAllMedia, {}, false, {}},
        {Old::customUser | Old::exportArchive, {{resourceId}}, false, {}},
        {Old::customUser | Old::viewBookmarks | Old::accessAllMedia, {}, false, {}},
        {Old::customUser | Old::viewBookmarks, {{resourceId}}, false, {}},
        {Old::customUser | Old::manageBookmarks | Old::accessAllMedia, {}, false, {}},
        {Old::customUser | Old::manageBookmarks, {{resourceId}}, false, {}},
        {Old::customUser | Old::userInput | Old::accessAllMedia, {}, false, {GlobalPermission::generateEvents}},
        {Old::customUser | Old::userInput, {{resourceId}}, false, {GlobalPermission::generateEvents}},
        {Old::customUser | Old::controlVideowall, {}, false, {}},
        {Old::accessAllMedia, {}, false, {}},
    };
    const auto userId = QnUuid::createUuid();
    for (const auto& origin: deprecatedPermissions)
    {
        GlobalPermissions permissions;
        std::vector<QnUuid> groups;
        std::map<QnUuid, AccessRights> resourceAccessRights;
        std::tie(permissions, groups, resourceAccessRights) = migrateAccessRights(
            origin.permissions, origin.accessibleResources.value_or(std::vector<QnUuid>{}));
        ASSERT_EQ(permissions, origin.newPermissions);
        PermissionsV1 convertedBack;
        std::tie(
            convertedBack.permissions, convertedBack.accessibleResources, convertedBack.isOwner) =
            extractFromResourceAccessRights(permissions, &groups, resourceAccessRights);
        ASSERT_EQ(nx::reflect::json::serialize(origin), nx::reflect::json::serialize(convertedBack));
    }
}

TEST(Permissions, CasesNotPreservingBackwardCompatibility)
{
    const std::vector<PermissionsV1> deprecatedPermissions{
        {Old::editCameras, {}, false, {}},
        {Old::viewArchive, {}, false, {}},
        {Old::exportArchive, {}, false, {}},
        {Old::viewBookmarks, {}, false, {}},
        {Old::manageBookmarks, {}, false, {}},
        {Old::userInput, {}, false, {GlobalPermission::generateEvents}},
    };
    for (const auto& origin: deprecatedPermissions)
    {
        GlobalPermissions permissions;
        std::vector<QnUuid> groups;
        std::map<QnUuid, AccessRights> resourceAccessRights;
        std::tie(permissions, groups, resourceAccessRights) = migrateAccessRights(
            origin.permissions, origin.accessibleResources.value_or(std::vector<QnUuid>{}));
        ASSERT_EQ(permissions, origin.newPermissions);
        PermissionsV1 convertedBack;
        std::tie(
            convertedBack.permissions, convertedBack.accessibleResources, convertedBack.isOwner) =
            extractFromResourceAccessRights(permissions, &groups, resourceAccessRights);
        PermissionsV1 expectedPermissions{Old::customUser};
        if (origin.newPermissions.testFlag(GlobalPermission::generateEvents))
            expectedPermissions.permissions |= Old::userInput;
        ASSERT_EQ(nx::reflect::json::serialize(expectedPermissions), nx::reflect::json::serialize(convertedBack));
    }
}

} // namespace nx::vms::api::test
