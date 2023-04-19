// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json/serializer.h>
#include <nx/vms/api/data/permission_converter.h>

namespace nx::vms::api::test {

struct PermissionsV1
{
    GlobalPermissions permissions = GlobalPermission::none;
    std::optional<std::vector<QnUuid>> accessibleResources;
};
NX_REFLECTION_INSTRUMENT(PermissionsV1, (permissions)(accessibleResources))

TEST(Permissions, BackwardCompatibility)
{
    const auto resourceId = QnUuid::createUuid();
    const std::vector<PermissionsV1> deprecatedPermissions{
        {GlobalPermission::editCameras | GlobalPermission::accessAllMedia, {}},
        {GlobalPermission::editCameras, {{resourceId}}},
        {GlobalPermission::viewArchive | GlobalPermission::accessAllMedia, {}},
        {GlobalPermission::viewArchive, {{resourceId}}},
        {GlobalPermission::exportArchive | GlobalPermission::accessAllMedia, {}},
        {GlobalPermission::exportArchive, {{resourceId}}},
        {GlobalPermission::viewBookmarks | GlobalPermission::accessAllMedia, {}},
        {GlobalPermission::viewBookmarks, {{resourceId}}},
        {GlobalPermission::manageBookmarks | GlobalPermission::accessAllMedia, {}},
        {GlobalPermission::manageBookmarks, {{resourceId}}},
        {GlobalPermission::userInput | GlobalPermission::accessAllMedia, {}},
        {GlobalPermission::userInput, {{resourceId}}},
        {GlobalPermission::controlVideowall, {}},
        {GlobalPermission::accessAllMedia, {}},
    };
    const auto userId = QnUuid::createUuid();
    for (const auto& origin: deprecatedPermissions)
    {
        auto permissions = origin.permissions;
        auto converted =
            PermissionConverter::accessRights(&permissions, userId, origin.accessibleResources);
        ASSERT_EQ(permissions, GlobalPermission::none);
        PermissionsV1 convertedBack;
        PermissionConverter::extractFromResourceAccessRights(
            {converted}, userId, &convertedBack.permissions, &convertedBack.accessibleResources);
        ASSERT_EQ(nx::reflect::json::serialize(origin), nx::reflect::json::serialize(convertedBack));
    }
}

TEST(Permissions, CasesNotPreservingBackwardCompatibility)
{
    const std::vector<PermissionsV1> deprecatedPermissions{
        {GlobalPermission::editCameras, {}},
        {GlobalPermission::viewArchive, {}},
        {GlobalPermission::exportArchive, {}},
        {GlobalPermission::viewBookmarks, {}},
        {GlobalPermission::manageBookmarks, {}},
        {GlobalPermission::userInput, {}},
    };
    const auto expectedNoPermissions = nx::reflect::json::serialize(PermissionsV1{});
    const auto userId = QnUuid::createUuid();
    for (const auto& origin: deprecatedPermissions)
    {
        auto permissions = origin.permissions;
        auto converted =
            PermissionConverter::accessRights(&permissions, userId, origin.accessibleResources);
        ASSERT_EQ(permissions, GlobalPermission::none);
        PermissionsV1 convertedBack;
        PermissionConverter::extractFromResourceAccessRights(
            {converted}, userId, &convertedBack.permissions, &convertedBack.accessibleResources);
        ASSERT_EQ(expectedNoPermissions, nx::reflect::json::serialize(convertedBack));
    }
}

} // namespace nx::vms::api::test
