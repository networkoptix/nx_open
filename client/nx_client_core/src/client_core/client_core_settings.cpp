#include "client_core_settings.h"

#include <QtCore/QLatin1String>
#include <QtCore/QSettings>

#include <nx/fusion/model_functions.h>

#include <nx/utils/raii_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>

namespace {

const QString kEncodeXorKey = lit("ItIsAGoodDayToDie");

const auto kCoreSettingsGroup = lit("client_core");

} //namespace

QnClientCoreSettings::QnClientCoreSettings(QObject* parent) :
    base_type(parent),
    m_settings(new QSettings(this))
{
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
        /* This value should not be modified by the client. */
        case CdbEndpoint:
            return;

        case RecentLocalConnections:
        {
            auto connections = value.value<RecentLocalConnectionsHash>();
            processedValue = QString::fromUtf8(QJson::serialized(connections));
            break;
        }
        case SystemAuthenticationData:
        {
            auto data = value.value<SystemAuthenticationDataHash>();
            processedValue = QString::fromUtf8(QJson::serialized(data));
            break;
        }
        case LocalSystemWeightsData:
        {
            auto list = value.value<WeightDataList>();
            processedValue = QString::fromUtf8(QJson::serialized(list));
            break;
        }
        case RecentCloudSystems:
        {
            auto list = value.value<QnCloudSystemList>();
            processedValue = QString::fromUtf8(QJson::serialized(list));
            break;
        }
        case CloudPassword:
        {
            processedValue = nx::utils::xorEncrypt(value.toString(), kEncodeXorKey);
            break;
        }
        case KnownServerConnections:
        {
            auto list = value.value<QList<KnownServerConnection>>();
            processedValue = QString::fromUtf8(QJson::serialized(list));
            break;
        }
        case KnownServerUrls:
        {
            auto list = value.value<QList<QUrl>>();
            processedValue = QString::fromUtf8(QJson::serialized(list));
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
    auto baseValue = base_type::readValueFromSettings(settings, id, defaultValue);
    switch (id)
    {
        case RecentLocalConnections:
            return qVariantFromValue(
                QJson::deserialized<RecentLocalConnectionsHash>(baseValue.toByteArray()));

        case SystemAuthenticationData:
            return qVariantFromValue(
                QJson::deserialized<SystemAuthenticationDataHash>(baseValue.toByteArray()));

        case LocalSystemWeightsData:
            return qVariantFromValue(
                QJson::deserialized<WeightDataList>(baseValue.toByteArray()));

        case RecentCloudSystems:
            return qVariantFromValue(
                QJson::deserialized<QnCloudSystemList>(baseValue.toByteArray()));

        case CloudPassword:
            return nx::utils::xorDecrypt(baseValue.toString(), kEncodeXorKey);

        case KnownServerConnections:
            return qVariantFromValue(
                QJson::deserialized<QList<KnownServerConnection>>(baseValue.toByteArray()));

        case KnownServerUrls:
            return qVariantFromValue(
                QJson::deserialized<QList<QUrl>>(baseValue.toByteArray()));

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
