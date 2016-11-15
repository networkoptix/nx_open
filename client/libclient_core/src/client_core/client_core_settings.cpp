#include "client_core_settings.h"

#include <QtCore/QLatin1String>
#include <QtCore/QSettings>

#include <nx/fusion/model_functions.h>

#include <nx/utils/raii_guard.h>
#include <nx/utils/string.h>


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
            auto list = value.value<QnLocalConnectionDataList>();
            processedValue = QString::fromUtf8(QJson::serialized(list));
            break;
        }
        case LocalSystemWeightsData:
        {
            auto list = value.value<QnWeightDataList>();
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
            processedValue = nx::utils::xorEncrypt(value.toString(), kEncodeXorKey);
            break;
        default:
            break;
    }

    base_type::writeValueToSettings(settings, id, processedValue);
}

QVariant QnClientCoreSettings::readValueFromSettings(
    QSettings* settings,
    int id,
    const QVariant& defaultValue)
{
    auto baseValue = base_type::readValueFromSettings(settings, id, defaultValue);
    switch (id)
    {
        case RecentLocalConnections:
            return qVariantFromValue(
                QJson::deserialized<QnLocalConnectionDataList>(baseValue.toByteArray()));

        case LocalSystemWeightsData:
            return qVariantFromValue(
                QJson::deserialized<QnWeightDataList>(baseValue.toByteArray()));

        case RecentCloudSystems:
            return qVariantFromValue(
                QJson::deserialized<QnCloudSystemList>(baseValue.toByteArray()));

        case CloudPassword:
            return nx::utils::xorDecrypt(baseValue.toString(), kEncodeXorKey);

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
