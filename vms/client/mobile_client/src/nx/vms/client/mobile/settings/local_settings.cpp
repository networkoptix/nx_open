// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_settings.h"

#include <QtCore/QStandardPaths>

#include <nx/utils/log/log.h>
#include <nx/utils/property_storage/filesystem_backend.h>

namespace nx::vms::client::mobile {

LocalSettings::LocalSettings():
    Storage(new nx::utils::property_storage::FileSystemBackend(
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
            + "/settings/local"))
{
    load();
    generateDocumentation("Local Settings", "");
}

int LocalSettings::getCameraLayoutCustomColumnCount(
    const UserDescriptor& userDescriptor,
    const LayoutDescriptor& layoutDescriptor) const
{
    const auto cameraLayoutCustomColumnCountsForUsersValue =
        cameraLayoutCustomColumnCountsForUsers();
    const auto userIt = cameraLayoutCustomColumnCountsForUsersValue.find(userDescriptor);
    if (userIt == cameraLayoutCustomColumnCountsForUsersValue.end())
        return kInvalidCameraLayoutCustomColumnsCount;

    const auto& cameraLayoutCustomColumnCounts = userIt.value();
    const auto layoutIt = cameraLayoutCustomColumnCounts.find(layoutDescriptor);
    if (layoutIt == cameraLayoutCustomColumnCounts.end())
        return kInvalidCameraLayoutCustomColumnsCount;

    return layoutIt.value();
}

void LocalSettings::setCameraLayoutCustomColumnCount(
    const UserDescriptor& userDescriptor,
    const LayoutDescriptor& layoutDescriptor,
    int columnsCount)
{
    if (!NX_ASSERT(columnsCount >= 0
        || columnsCount == LocalSettings::kInvalidCameraLayoutCustomColumnsCount))
    {
        columnsCount = LocalSettings::kInvalidCameraLayoutCustomColumnsCount;
    }

    auto cameraLayoutCustomColumnCountsForUsersValue = cameraLayoutCustomColumnCountsForUsers();
    auto& cameraLayoutCustomColumnCounts =
        cameraLayoutCustomColumnCountsForUsersValue[userDescriptor];

    if (columnsCount != kInvalidCameraLayoutCustomColumnsCount)
        cameraLayoutCustomColumnCounts[layoutDescriptor] = columnsCount;
    else
        cameraLayoutCustomColumnCounts.remove(layoutDescriptor);

    cameraLayoutCustomColumnCountsForUsers.setValue(
        cameraLayoutCustomColumnCountsForUsersValue);
}

int LocalSettings::getCameraLayoutPosition(
    const UserDescriptor& userDescriptor,
    const LayoutDescriptor& layoutDescriptor) const
{
    const auto cameraLayoutPositionsForUsersValue = cameraLayoutPositionsForUsers();
    const auto userIt = cameraLayoutPositionsForUsersValue.find(userDescriptor);
    if (userIt == cameraLayoutPositionsForUsersValue.end())
        return kDefaultCameraLayoutPosition;

    const auto& cameraLayoutPositionsForUser = userIt.value();
    const auto layoutIt = cameraLayoutPositionsForUser.find(layoutDescriptor);
    if (layoutIt == cameraLayoutPositionsForUser.end())
        return kDefaultCameraLayoutPosition;

    return layoutIt.value();
}

void LocalSettings::setCameraLayoutPosition(
    const UserDescriptor& userDescriptor,
    const LayoutDescriptor& layoutDescriptor,
    int position)
{
    if (!NX_ASSERT(position >= 0))
        position = kDefaultCameraLayoutPosition;

    auto cameraLayoutPositionsForUsersValue = cameraLayoutPositionsForUsers();
    auto& cameraLayoutPositionsForUser =
        cameraLayoutPositionsForUsersValue[userDescriptor];

    if (position != kDefaultCameraLayoutPosition)
        cameraLayoutPositionsForUser[layoutDescriptor] = position;
    else
        cameraLayoutPositionsForUser.remove(layoutDescriptor);

    cameraLayoutPositionsForUsers.setValue(cameraLayoutPositionsForUsersValue);
}

} // namespace nx::vms::client::mobile
