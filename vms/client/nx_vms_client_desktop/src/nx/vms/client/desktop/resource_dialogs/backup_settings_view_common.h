// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <QtCore/QString>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types/resource_types.h>

class QMenu;
class QModelIndex;

namespace nx::vms::client::desktop {
namespace backup_settings_view {

bool isNewAddedCamerasRow(const QModelIndex& index);
bool isDropdownColumn(const QModelIndex& index);

bool hasDropdownData(const QModelIndex& index);
bool hasDropdownMenu(const QModelIndex& index);

bool isBackupSupported(const QnVirtualCameraResourcePtr& camera);
bool isBackupSupported(const QModelIndex& index);

std::unique_ptr<QMenu> createContentTypesMenu(
    bool isCloudBackupStorage,
    std::optional<nx::vms::api::BackupContentTypes> activeOption = std::nullopt);

std::unique_ptr<QMenu> createQualityMenu(
    std::optional<nx::vms::api::CameraBackupQuality> activeOption = std::nullopt);

QString backupContentTypesToString(nx::vms::api::BackupContentTypes backupContentTypes);
QString backupQualityToString(nx::vms::api::CameraBackupQuality backupQuality);

} // namespace backup_settings_view
} // namespace nx::vms::client::desktop
