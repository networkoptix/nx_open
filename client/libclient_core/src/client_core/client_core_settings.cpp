#include "client_core_settings.h"

#include <QtCore/QLatin1String>
#include <QtCore/QSettings>

#include <nx/utils/raii_guard.h>
#include <nx/utils/string.h>

namespace
{
    const auto kCoreSettingsGroup = lit("client_core");

    const auto kUserConnectionsSectionTag = lit("UserRecentConnections");
    const auto kRecentCloudSystemsTag = lit("recentCloudSystems");

    template<typename DataList, typename WriteSingleDataFunc>
    void writeListData(QSettings* settings, const DataList& data,
        const QString& tag, const WriteSingleDataFunc& writeSingle)
    {
        settings->beginWriteArray(tag);
        const auto arrayWriteGuard = QnRaiiGuard::createDestructable(
            [settings]() { settings->endArray(); });

        settings->remove(QString());  // Clear children
        const auto dataCount = data.size();
        auto it = data.begin();
        for (int i = 0; i != dataCount; ++i, ++it)
        {
            settings->setArrayIndex(i);
            writeSingle(settings, *it);
        }
    }

    template<typename DataList, typename ReadSingleDataFunc>
    DataList readListData(QSettings* settings, const QString& tag,
        const ReadSingleDataFunc& readSingleData)
    {
        DataList result;
        const int count = settings->beginReadArray(tag);
        const auto endArrayGuard = QnRaiiGuard::createDestructable(
            [settings]() { settings->endArray(); });

        for (int i = 0; i != count; ++i)
        {
            settings->setArrayIndex(i);
            result.append(readSingleData(settings));
        }
        return result;
    }

    void writeRecentUserConnections(QSettings* settings,
        const QnUserRecentConnectionDataList& connections)
    {
        writeListData(settings, connections, kUserConnectionsSectionTag,
            [](QSettings* settings, const QnUserRecentConnectionData& data)
            {
                QnUserRecentConnectionData::writeToSettings(settings, data);
            });
    }

    QnUserRecentConnectionDataList readRecentUserConnections(QSettings* settings)
    {
        return readListData<QnUserRecentConnectionDataList>(settings, kUserConnectionsSectionTag,
            [](QSettings* settings)
            {
                return QnUserRecentConnectionData::fromSettings(settings);
            });
    }

    void writeRecentCloudSystems(QSettings* settings, const QnCloudSystemList& systems)
    {
        writeListData(settings, systems, kRecentCloudSystemsTag,
            [](QSettings* settings, const QnCloudSystem& data)
            {
                QnCloudSystem::writeToSettings(settings, data);
            });
    }

    QnCloudSystemList readRecentCloudSystems(QSettings* settings)
    {
        return readListData<QnCloudSystemList>(settings, kRecentCloudSystemsTag,
            [](QSettings *settings)
            {
                return QnCloudSystem::fromSettings(settings);
            });
    }
}

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
        QSettings* settings, int id, const QVariant& value) const
{
    switch (id)
    {
        case RecentUserConnections:
            writeRecentUserConnections(settings, value.value<QnUserRecentConnectionDataList>());
            break;
        case RecentCloudSystems:
            writeRecentCloudSystems(settings, value.value<QnCloudSystemList>());
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
        case RecentUserConnections:
            return QVariant::fromValue<QnUserRecentConnectionDataList>(
                readRecentUserConnections(settings));
        case RecentCloudSystems:
            return QVariant::fromValue<QnCloudSystemList>(
                readRecentCloudSystems(settings));
        default:
            return base_type::readValueFromSettings(settings, id, defaultValue);
    }
}

void QnClientCoreSettings::save()
{
    submitToSettings(m_settings);
    m_settings->sync();
}
