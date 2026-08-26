// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_settings_adaptor.h"

#include <ini.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/ini_helpers.h>
#include <nx/vms/client/mobile/settings/local_settings.h>

namespace nx::client::mobile {

QmlSettingsAdaptor::QmlSettingsAdaptor(QObject* parent):
    QQmlPropertyMap(this, parent)
{
    // Set mobile settings values.
    for (int id: qnSettings->variables())
        insert(qnSettings->name(id), qnSettings->value(id));

    connect(qnSettings,
        &QnMobileClientSettings::valueChanged,
        this,
        [this](int id) { insert(qnSettings->name(id), qnSettings->value(id)); });

    // Subscribe to changes from the QML side and map them to mobile settings.
    connect(this,
        &QQmlPropertyMap::valueChanged,
        this,
        [](const QString& key, const QVariant& value)
        {
            if (!qnSettings->setValue(key, value))
                return;

            qnSettings->save();
        });

    auto updateFromSettings = [this](auto* property)
    {
        if (property && !property->secure)
            insert(property->name, property->variantValue());
    };

    auto addStorage = [this, updateFromSettings](nx::utils::property_storage::Storage* storage)
    {
        if (!NX_ASSERT(storage))
            return;

        for (const auto& property: storage->properties())
        {
            NX_ASSERT(!contains(property->name), "Settings name conflict: %1", property->name);
            updateFromSettings(property);
        }

        connect(storage, &nx::utils::property_storage::Storage::changed, this, updateFromSettings);

        // Subscribe to changes from the QML side and map them to the storage.
        connect(this,
            &QQmlPropertyMap::valueChanged,
            this,
            [storage](const QString& key, const QVariant& value)
            { storage->setValue(key, value); });
    };

    addStorage(nx::vms::client::core::appContext()->coreSettings());
    addStorage(nx::vms::client::mobile::appContext()->localSettings());
}

QVariant QmlSettingsAdaptor::iniConfigValue(const QString& name)
{
    return nx::vms::client::core::getIniValue(mobile_client::ini(), name);
}

} // namespace nx::client::mobile
