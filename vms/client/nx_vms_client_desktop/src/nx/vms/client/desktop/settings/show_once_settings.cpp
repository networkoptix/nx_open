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
    migrate();
}

void ShowOnceSettings::reload()
{
    load();
}

void ShowOnceSettings::reset()
{
    for (BaseProperty* property: properties())
        property->setVariantValue(false);
}

void ShowOnceSettings::migrate()
{
    if (migrationDone())
        return;

    auto oldSettings = std::make_unique<QSettings>();
    oldSettings->beginGroup("show_once");

    cloudLayoutsPromo = oldSettings->value("CloudLayoutsPromo").toBool();
    cloudPromo = oldSettings->value("CloudPromoNotification").toBool();
    digestDisableNotification = oldSettings->value("DigestDisableNotification").toBool();
    sharedLayoutEdit = oldSettings->value("SharedLayoutEdit").toBool();
    removeItemsFromLayout = oldSettings->value("RemoveItemsFromLayout").toBool();
    removeItemsFromShowreel = oldSettings->value("RemoveItemsFromShowreel").toBool();
    deleteResources = oldSettings->value("DeleteResources").toBool();
    mergeResourceGroups = oldSettings->value("MergeResourceGroups").toBool();
    moveProxiedWebpageWarning = oldSettings->value("MoveProxiedWebpageWarning").toBool();
    deleteLocalLayouts = oldSettings->value("DeleteLocalLayouts").toBool();
    ptzPresetInUse = oldSettings->value("PtzPresetInUse").toBool();
    newPtzMechanicPromo = oldSettings->value("NewPtzMechanicPromoBlock").toBool();
    autoTrackingPromo = oldSettings->value("AutoTrackingPromoBlock").toBool();
    versionMismatch = oldSettings->value("NewPtzMechanicPromoBlock").toBool();

    migrationDone = true;
}

ShowOnceSettings* showOnceSettings()
{
    return appContext()->showOnceSettings();
}

} // namespace nx::vms::client::desktop
