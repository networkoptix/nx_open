// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_core_settings.h"

#include <QtCore/QLatin1String>
#include <QtCore/QSettings>

#include <nx/fusion/model_functions.h>
#include <nx/reflect/json.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>

using KnownServerConnection = nx::vms::client::core::watchers::KnownServerConnections::Connection;

namespace {

const QString kEncodeXorKey = lit("ItIsAGoodDayToDie");

const auto kCoreSettingsGroup = lit("client_core");

// Cleanup invalid or corrupted stored connections.
QList<KnownServerConnection> validKnownConnections(QList<KnownServerConnection> source)
{
    QList<KnownServerConnection> result;
    for (auto connection: source)
    {
        if (connection.url.isValid() && !connection.url.host().isEmpty())
            result.push_back(connection);
    }
    return result;
}

} //namespace

template<> QnClientCoreSettings* Singleton<QnClientCoreSettings>::s_instance = nullptr;

QnClientCoreSettings::QnClientCoreSettings(QObject* parent) :
    base_type(parent),
    m_settings(new QSettings(this))
{
    qRegisterMetaType<QnStringSet>("QnStringSet");
    qRegisterMetaTypeStreamOperators<QnStringSet>();

    m_settings->beginGroup(kCoreSettingsGroup);
    init();
    updateFromSettings(m_settings);
}

QnClientCoreSettings::~QnClientCoreSettings()
{
    submitToSettings(m_settings);
}

void QnClientCoreSettings::writeValueToSettings(
    QSettings* settings,
    int id,
    const QVariant& value) const
{
    auto processedValue = value;
    switch (id)
    {
        case RecentLocalConnections:
        {
            const auto connections = value.value<RecentLocalConnectionsHash>();
            processedValue = QString::fromUtf8(QJson::serialized(connections));
            break;
        }
        case SearchAddresses:
        {
            const auto data = value.value<SystemSearchAddressesHash>();
            processedValue = QString::fromUtf8(QJson::serialized(data));
            break;
        }
        case LocalSystemWeightsData:
        {
            const auto list = value.value<WeightDataList>();
            processedValue = QString::fromUtf8(QJson::serialized(list));
            break;
        }
        case RecentCloudSystems:
        {
            const auto list = value.value<QnCloudSystemList>();
            processedValue = QString::fromUtf8(QJson::serialized(list));
            break;
        }
        case KnownServerConnections:
        {
            const auto list = value.value<QList<KnownServerConnection>>();
            processedValue = QString::fromStdString(nx::reflect::json::serialize(list));
            break;
        }
        case TileScopeInfo:
        {
            const auto infos = value.value<SystemVisibilityScopeInfoHash>();
            processedValue = QString::fromUtf8(QJson::serialized(infos));
            break;
        }
        default:
            break;
    }

    base_type::writeValueToSettings(settings, id, processedValue);
}

QVariant QnClientCoreSettings::readValueFromSettings(
    QSettings* settings,
    int id,
    const QVariant& defaultValue) const
{
    const auto baseValue = base_type::readValueFromSettings(settings, id, defaultValue);
    switch (id)
    {
        case RecentLocalConnections:
        {
            return QVariant::fromValue(
                QJson::deserialized<RecentLocalConnectionsHash>(baseValue.toByteArray()));
        }

        case SearchAddresses:
        {
            return QVariant::fromValue(
                QJson::deserialized<SystemSearchAddressesHash>(baseValue.toByteArray()));
        }

        case LocalSystemWeightsData:
        {
            return QVariant::fromValue(
                QJson::deserialized<WeightDataList>(baseValue.toByteArray()));
        }

        case RecentCloudSystems:
        {
            return QVariant::fromValue(
                QJson::deserialized<QnCloudSystemList>(baseValue.toByteArray()));
        }

        case KnownServerConnections:
        {
            auto [connections, _] = nx::reflect::json::deserialize<QList<KnownServerConnection>>(
                baseValue.toByteArray().toStdString());
            return QVariant::fromValue(validKnownConnections(connections));
        }

        case TileScopeInfo:
        {
            return QVariant::fromValue(
                QJson::deserialized<SystemVisibilityScopeInfoHash>(baseValue.toByteArray()));
        }

        default:
            break;
    }

    return baseValue;
}

void QnClientCoreSettings::save()
{
    submitToSettings(m_settings);
    m_settings->sync();
}
