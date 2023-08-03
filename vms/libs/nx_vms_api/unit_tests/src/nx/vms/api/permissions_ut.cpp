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
};
NX_REFLECTION_INSTRUMENT(PermissionsV1, (permissions)(accessibleResources))

TEST(Permissions, BackwardCompatibility)
{
    const auto resourceId = QnUuid::createUuid();
    const std::vector<PermissionsV1> deprecatedPermissions{
        {Old::customUser | Old::editCameras | Old::accessAllMedia, {}},
        {Old::customUser | Old::editCameras, {{resourceId}}},
        {Old::customUser | Old::viewArchive | Old::accessAllMedia, {}},
        {Old::customUser | Old::viewArchive, {{resourceId}}},
        {Old::customUser | Old::exportArchive | Old::accessAllMedia, {}},
        {Old::customUser | Old::exportArchive, {{resourceId}}},
        {Old::customUser | Old::viewBookmarks | Old::accessAllMedia, {}},
        {Old::customUser | Old::viewBookmarks, {{resourceId}}},
        {Old::customUser | Old::manageBookmarks | Old::accessAllMedia, {}},
        {Old::customUser | Old::manageBookmarks, {{resourceId}}},
        {Old::customUser | Old::userInput | Old::accessAllMedia, {}},
        {Old::customUser | Old::userInput, {{resourceId}}},
        {Old::customUser | Old::controlVideowall, {}},
        {Old::accessAllMedia, {}},
    };
    const auto userId = QnUuid::createUuid();
    for (const auto& origin: deprecatedPermissions)
    {
        GlobalPermissions permissions;
        std::vector<QnUuid> groups;
        std::map<QnUuid, AccessRights> resourceAccessRights;
        std::tie(permissions, groups, resourceAccessRights) = migrateAccessRights(
            origin.permissions, origin.accessibleResources.value_or(std::vector<QnUuid>{}));
        ASSERT_EQ(permissions, GlobalPermission::none);
        PermissionsV1 convertedBack;
        std::tie(
            convertedBack.permissions, convertedBack.accessibleResources, convertedBack.isOwner) =
            extractFromResourceAccessRights(permissions, std::move(groups), resourceAccessRights);
        ASSERT_EQ(nx::reflect::json::serialize(origin), nx::reflect::json::serialize(convertedBack));
    }
}

TEST(Permissions, CasesNotPreservingBackwardCompatibility)
{
    const std::vector<PermissionsV1> deprecatedPermissions{
        {Old::editCameras, {}},
        {Old::viewArchive, {}},
        {Old::exportArchive, {}},
        {Old::viewBookmarks, {}},
        {Old::manageBookmarks, {}},
        {Old::userInput, {}},
    };
    const auto expectedNoPermissions = nx::reflect::json::serialize(PermissionsV1{Old::customUser});
    for (const auto& origin: deprecatedPermissions)
    {
        GlobalPermissions permissions;
        std::vector<QnUuid> groups;
        std::map<QnUuid, AccessRights> resourceAccessRights;
        std::tie(permissions, groups, resourceAccessRights) = migrateAccessRights(
            origin.permissions, origin.accessibleResources.value_or(std::vector<QnUuid>{}));
        ASSERT_EQ(permissions, GlobalPermission::none);
        PermissionsV1 convertedBack;
        std::tie(
            convertedBack.permissions, convertedBack.accessibleResources, convertedBack.isOwner) =
            extractFromResourceAccessRights(permissions, std::move(groups), resourceAccessRights);
        ASSERT_EQ(expectedNoPermissions, nx::reflect::json::serialize(convertedBack));
    }
}

} // namespace nx::vms::api::test
