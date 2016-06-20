#include "client_core_settings.h"

#include <QtCore/QLatin1String>
#include <QtCore/QSettings>

#include <nx/utils/raii_guard.h>
#include <utils/common/string.h>

namespace
{
    const auto kCoreSettingsGroup = lit("client_core");
    
    const auto kUserConnectionsSectionTag = lit("UserRecentConnections");

    void writeRecentUserConnections(
            QSettings*settings,
            const QnUserRecentConnectionDataList& connections)
    {
        settings->beginWriteArray(kUserConnectionsSectionTag);
        const auto arrayWriteGuard = QnRaiiGuard::createDestructable(
            [settings]() { settings->endArray(); });
        
        settings->remove(QString());  // Clear children
        const auto connCount = connections.size();
        for (int i = 0; i != connCount; ++i)
        {
            settings->setArrayIndex(i);
            QnUserRecentConnectionData::writeToSettings(settings
                , connections.at(i));
        }
    }

    QnUserRecentConnectionDataList readRecentUserConnections(QSettings* settings)
    {
        QnUserRecentConnectionDataList result;
        const int count = settings->beginReadArray(kUserConnectionsSectionTag);
        const auto endArrayGuard = QnRaiiGuard::createDestructable(
            [settings]() { settings->endArray(); });

        for (int i = 0; i != count; ++i)
        {
            settings->setArrayIndex(i);
            result.append(QnUserRecentConnectionData::fromSettings(settings));
        }
        return result;
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
            writeRecentUserConnections(
                        settings, value.value<QnUserRecentConnectionDataList>());
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
        default:
            return base_type::readValueFromSettings(settings, id, defaultValue);
    }
}

void QnClientCoreSettings::save()
{
    submitToSettings(m_settings);
    m_settings->sync();
}
