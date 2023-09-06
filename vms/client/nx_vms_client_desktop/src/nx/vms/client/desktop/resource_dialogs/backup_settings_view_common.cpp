// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_view_common.h"

#include <array>

#include <QtCore/QCoreApplication>
#include <QtCore/QModelIndex>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/style/helper.h>

using namespace nx::vms::api;

namespace {

static constexpr std::array<BackupContentTypes, 8> kBackupContentTypesMenuOptions = {
    BackupContentType::archive,
    BackupContentType::motion,
    BackupContentType::analytics,
    BackupContentType::bookmarks,
    BackupContentType::motion | BackupContentType::analytics,
    BackupContentType::motion | BackupContentType::bookmarks,
    BackupContentType::analytics | BackupContentType::bookmarks,
    BackupContentType::motion | BackupContentType::analytics | BackupContentType::bookmarks};

static constexpr std::array<CameraBackupQuality, 2> kBackupQualityMenuOptions = {
    CameraBackupQuality::CameraBackupBoth,
    CameraBackupQuality::CameraBackupLowQuality};

} // namespace

namespace nx::vms::client::desktop {
namespace backup_settings_view {

class BackupSettingsViewStrings
{
    Q_DECLARE_TR_FUNCTIONS(
        nx::vms::client::desktop::backup_settings_view::BackupSettingsViewStrings)

public:
    static QString backupContentTypesToString(BackupContentTypes contentTypes)
    {
        if (contentTypes.testFlag(BackupContentType::archive))
            return tr("All archive");

        QStringList contentTypesStrings;

        if (contentTypes.testFlag(BackupContentType::motion))
            contentTypesStrings.push_back(tr("Motion"));

        if (contentTypes.testFlag(BackupContentType::analytics))
            contentTypesStrings.push_back(tr("Objects"));

        if (contentTypes.testFlag(BackupContentType::bookmarks))
            contentTypesStrings.push_back(tr("Bookmarks"));

        return contentTypesStrings.join(", ");
    }

    static QString backupQualityToString(CameraBackupQuality backupQuality)
    {
        switch (backupQuality)
        {
            case CameraBackupHighQuality:
                return tr("High-Res");

            case CameraBackupLowQuality:
                return tr("Low-Res");

            case CameraBackupBoth:
                return tr("All streams");

            default:
                break;
        }

        NX_ASSERT(false, "Unexpected backup quality parameter");
        return QString();
    }
};

bool isNewAddedCamerasRow(const QModelIndex& index)
{
    if (!index.isValid())
        return false;

    const auto resourceIndex = index.siblingAtColumn(ResourceColumn);
    const auto newAdddedCamerasData = resourceIndex.data(NewAddedCamerasItemRole);
    return !newAdddedCamerasData.isNull() && newAdddedCamerasData.toBool();
}

bool isDropdownColumn(const QModelIndex& index)
{
    if (!index.isValid())
        return false;

    return index.column() == ContentTypesColumn || index.column() == QualityColumn;
}

bool hasDropdownData(const QModelIndex& index)
{
    if (!index.isValid())
        return false;

    return !index.data(BackupContentTypesRole).isNull() || !index.data(BackupQualityRole).isNull();
}

bool hasDropdownMenu(const QModelIndex& index)
{
    return index.data(HasDropdownMenuRole).toBool();
}

bool isBackupSupported(const QnVirtualCameraResourcePtr& camera)
{
    if (camera.isNull())
        return false;

    if (camera->isDtsBased())
        return false;

    return true;
}

bool isBackupSupported(const QModelIndex& index)
{
    if (!index.isValid())
        return false;

    const auto resourceIndex = index.siblingAtColumn(ResourceColumn);

    return isBackupSupported(resourceIndex.data(Qn::ResourceRole).value<QnResourcePtr>()
        .dynamicCast<QnVirtualCameraResource>());
}

QString backupContentTypesToString(BackupContentTypes contentTypes)
{
    return BackupSettingsViewStrings::backupContentTypesToString(contentTypes);
}

QString backupQualityToString(CameraBackupQuality backupQuality)
{
    return BackupSettingsViewStrings::backupQualityToString(backupQuality);
}

std::unique_ptr<QMenu> createContentTypesMenu(std::optional<BackupContentTypes> activeOption)
{
    auto menu = std::make_unique<QMenu>();
    for (const auto& contentTypesOption: kBackupContentTypesMenuOptions)
    {
        auto action = menu->addAction(backupContentTypesToString(contentTypesOption));
        action->setData(QVariant::fromValue<int>(contentTypesOption));
        if (activeOption && *activeOption == contentTypesOption)
            menu->setActiveAction(action);
    }
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    return menu;
}

std::unique_ptr<QMenu> createQualityMenu(std::optional<CameraBackupQuality> activeOption)
{
    auto menu = std::make_unique<QMenu>();
    for (const auto& qualityOption: kBackupQualityMenuOptions)
    {
        auto action = menu->addAction(backupQualityToString(qualityOption));
        action->setData(QVariant::fromValue<int>(qualityOption));
        if (activeOption && *activeOption == qualityOption)
            menu->setActiveAction(action);
    }
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    return menu;
}

} // namespace backup_settings_view
} // namespace nx::vms::client::desktop
