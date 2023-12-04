// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_once_settings.h"

#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>

#include <nx/utils/property_storage/filesystem_backend.h>
#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop {

ShowOnceSettings::ShowOnceSettings():
    Storage(new nx::utils::property_storage::FileSystemBackend(
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
            + "/settings/show_once"
    ))
{
    load();
    migrate();
}

void ShowOnceSettings::reload()
{
    load();
}

void ShowOnceSettings::reset()
{
    for (BaseProperty* property: properties())
    {
        if (property != &migrationDone)
            property->setVariantValue(false);
    }
}

void ShowOnceSettings::migrate()
{
    if (migrationDone())
        return;

    auto oldSettings = std::make_unique<QSettings>();

    const auto readValue =
        [&oldSettings](const QString& name)
        {
            return oldSettings->value("show_once/" + name).toBool();
        };

    cloudLayoutsPromo = readValue("CloudLayoutsPromo");
    cloudPromo = readValue("CloudPromoNotification");
    digestDisableNotification = readValue("DigestDisableNotification");
    sharedLayoutEdit = readValue("SharedLayoutEdit");
    removeItemsFromLayout = readValue("RemoveItemsFromLayout");
    deleteResources = readValue("DeleteResources");
    mergeResourceGroups = readValue("MergeResourceGroups");
    moveProxiedWebpageWarning = readValue("MoveProxiedWebpageWarning");
    deleteLocalLayouts = readValue("DeleteLocalLayouts");
    ptzPresetInUse = readValue("PtzPresetInUse");
    newPtzMechanicPromo = readValue("NewPtzMechanicPromoBlock");
    versionMismatch = readValue("VersionMismatch");

    migrationDone = true;
}

ShowOnceSettings* showOnceSettings()
{
    return appContext()->showOnceSettings();
}

} // namespace nx::vms::client::desktop
