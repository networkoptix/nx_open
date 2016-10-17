#include "client_core_settings.h"

#include <QtCore/QLatin1String>
#include <QtCore/QSettings>

#include <nx/utils/raii_guard.h>
#include <nx/utils/string.h>

namespace {

const QString kEncodeXorKey = lit("ItIsAGoodDayToDie");

const auto kCoreSettingsGroup = lit("client_core");

const auto kRecentLocalConnectionsTag = lit("UserRecentConnections");
const auto kRecentCloudSystemsTag = lit("recentCloudSystems");
const auto kLocalSystemWeights = lit("localSystemWeights");

template<typename DataType>
void writeListData(QSettings* settings, const QVariant& value, const QString& tag)
{
    const auto data = value.value<QList<DataType>>();
    settings->beginWriteArray(tag);
    const auto arrayWriteGuard = QnRaiiGuard::createDestructable([settings]() { settings->endArray(); });

    settings->remove(QString());  // Clear children
    const auto dataCount = data.size();
    auto it = data.begin();
    for (int i = 0; i != dataCount; ++i, ++it)
    {
        settings->setArrayIndex(i);
        it->writeToSettings(settings);
    }
}

template<typename ResultType>
QVariant readListData(QSettings* settings, const QString& tag)
{
    QList<ResultType> result;
    const int count = settings->beginReadArray(tag);
    const auto endArrayGuard = QnRaiiGuard::createDestructable([settings]() { settings->endArray(); });

    for (int i = 0; i != count; ++i)
    {
        settings->setArrayIndex(i);
        result.append(ResultType::fromSettings(settings));
    }

    return QVariant::fromValue(result);
}

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
    switch (id)
    {
        case RecentLocalConnections:
            writeListData<QnLocalConnectionData>(settings, value, kRecentLocalConnectionsTag);
            break;
        case LocalSystemWeightsData:
            writeListData<QnWeightData>(settings, value, kLocalSystemWeights);
            break;
        case RecentCloudSystems:
            writeListData<QnCloudSystem>(settings, value, kRecentCloudSystemsTag);
            break;
        case CloudPassword:
            base_type::writeValueToSettings(settings, id, nx::utils::xorEncrypt(value.toString(),
                kEncodeXorKey));
            break;
        default:
            base_type::writeValueToSettings(settings, id, value);
    }
}

QVariant QnClientCoreSettings::readValueFromSettings(
    QSettings* settings,
    int id,
    const QVariant& defaultValue)
{
    switch (id)
    {
        case RecentLocalConnections:
            return readListData<QnLocalConnectionData>(settings, kRecentLocalConnectionsTag);
        case LocalSystemWeightsData:
            return readListData<QnWeightData>(settings, kLocalSystemWeights);
        case RecentCloudSystems:
            return readListData<QnCloudSystem>(settings, kRecentCloudSystemsTag);
        case CloudPassword:
            return nx::utils::xorDecrypt(
                base_type::readValueFromSettings(settings, id, defaultValue).toString(),
                kEncodeXorKey);
        default:
            return base_type::readValueFromSettings(settings, id, defaultValue);
    }
}

void QnClientCoreSettings::save()
{
    submitToSettings(m_settings);
    m_settings->sync();
}
