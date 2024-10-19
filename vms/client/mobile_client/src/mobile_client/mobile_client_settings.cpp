// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_settings.h"

#include <QtCore/QSettings>
#include <QtCore/QScopedValueRollback>

#include <nx/reflect/json.h>

template<> QnMobileClientSettings* Singleton<QnMobileClientSettings>::s_instance = nullptr;

QnMobileClientSettings::QnMobileClientSettings(QObject* parent) :
    base_type(parent),
    m_settings(new QSettings(this)),
    m_loading(true)
{
    init();
    load();
    setThreadSafe(true);
    m_loading = false;
}

void QnMobileClientSettings::load()
{
    updateFromSettings(m_settings);
}

void QnMobileClientSettings::save()
{
    submitToSettings(m_settings);
}

bool QnMobileClientSettings::isWritable() const
{
    return m_settings->isWritable();
}

void QnMobileClientSettings::updateValuesFromSettings(
        QSettings* settings,
        const QList<int>& ids)
{
    QScopedValueRollback<bool> guard(m_loading, true);

    base_type::updateValuesFromSettings(settings, ids);
}

QVariant QnMobileClientSettings::readValueFromSettings(
        QSettings* settings,
        int id,
        const QVariant& defaultValue) const
{
    auto baseValue = base_type::readValueFromSettings(settings, id, defaultValue);

    switch (id)
    {
        case UserPushSettings:
        {
            nx::vms::client::mobile::details::UserPushSettings result;
            if (nx::reflect::json::deserialize(baseValue.toString().toStdString(), &result))
                return QVariant::fromValue(result);
            break;
        }

        case RemoteLogSession:
        {
            nx::vms::client::mobile::RemoteLogSessionData result;
            if (nx::reflect::json::deserialize(baseValue.toString().toStdString(), &result))
                return QVariant::fromValue(result);
            break;
        }

        default:
            break;
    }

    return baseValue;
}

void QnMobileClientSettings::writeValueToSettings(
    QSettings* settings,
    int id,
    const QVariant& value) const
{
    auto processedValue = value;

    switch (id)
    {
        /* Temporary options. Not to be written. */
        case StartupParameters:
            return;

        case UserPushSettings:
        {
            const auto data = value.value<nx::vms::client::mobile::details::UserPushSettings>();
            processedValue = QString::fromStdString(nx::reflect::json::serialize(data));
            break;
        }

        case RemoteLogSession:
        {
            const auto data = value.value<nx::vms::client::mobile::RemoteLogSessionData>();
            processedValue = QString::fromStdString(nx::reflect::json::serialize(data));
            break;
        }

        default:
            break;
    }

    base_type::writeValueToSettings(settings, id, processedValue);
}

QnPropertyStorage::UpdateStatus QnMobileClientSettings::updateValue(
        int id,
        const QVariant& value)
{
    const auto status = base_type::updateValue(id, value);

    /* Settings are to be written out right away. */
    if (!m_loading && status == Changed)
        writeValueToSettings(m_settings, id, this->value(id));

    return status;
}
