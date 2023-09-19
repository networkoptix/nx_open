// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_bar_settings.h"

#include <QtCore/QStandardPaths>

#include <nx/utils/property_storage/filesystem_backend.h>
#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop {

MessageBarSettings::MessageBarSettings():
    Storage(new nx::utils::property_storage::FileSystemBackend(
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
            + "/settings/message_bar"
    ))
{
    load();
}

void MessageBarSettings::reload()
{
    load();
}

void MessageBarSettings::reset()
{
    for (BaseProperty* property: properties())
        property->setVariantValue(true);
}

MessageBarSettings* messageBarSettings()
{
    return appContext()->messageBarSettings();
}

} // namespace nx::vms::client::desktop
