// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_settings_adaptor.h"

#include <ini.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/ini_helpers.h>

using nx::vms::client::core::appContext;

namespace nx::client::mobile {

QmlSettingsAdaptor::QmlSettingsAdaptor(QObject* parent):
    QQmlPropertyMap(parent)
{
    // Set mobile settings values.
    const auto mobileIds = qnSettings->variables();
    QHash<QString, int> mobileProperties;
    for (int id: mobileIds)
    {
        const auto name = qnSettings->name(id);
        mobileProperties.insert(name, id);
        insert(name, qnSettings->value(id));
    }

    // Set core settings values.
    const auto coreProperties = appContext()->coreSettings()->properties();
    for (const auto&[key, value]: coreProperties.asKeyValueRange())
    {
        NX_ASSERT(!mobileProperties.contains(key), "Mobile and core settings conflict");
        insert(key, value->variantValue());
    }

    // Subscribe to mobile settings changes.
    connect(qnSettings, &QnMobileClientSettings::valueChanged, this,
        [this](int id)
        {
            insert(qnSettings->name(id), qnSettings->value(id));
        });

    // Subscribe to core settings changes.
    connect(appContext()->coreSettings(), &vms::client::core::Settings::changed, this,
        [this](const auto property)
        {
            if (!property)
                return;

            insert(property->name, property->variantValue());
        });

    // Subscribe to changes from QML side and map them to either core or mobile.
    connect(this, &QQmlPropertyMap::valueChanged,
        [mobileProperties=std::move(mobileProperties)](const QString& key, const QVariant& value)
        {
            if (auto it = mobileProperties.find(key); it != mobileProperties.end())
            {
                qnSettings->setValue(it.value(), value);
                qnSettings->save();
                return;
            }

            appContext()->coreSettings()->setValue(key, value);
        });
}

} // namespace nx::client::mobile
