
#include "core_settings.h"

#include <QtCore/QLatin1String>
#include <QtCore/QSettings>

#include <nx/utils/raii_guard.h>
#include <utils/common/string.h>

namespace
{
    const auto kCoreSettingsFile = lit("core_client_settings");
    
    const auto kUserConnectionsSectionTag = lit("UserRecentConnections");
    const auto kXorKey = lit("thereIsSomeKeyForXorOperation");

    const auto kUserNameTag = lit("userName");
    const auto kPasswordTag = lit("password");
    const auto kSystemNameTag = lit("systemName");
    const auto kStoredPassword = lit("storedPassword");

    void writeRecentUserConnections(QSettings *settings
        , const QnUserRecentConnectionDataList &connections)
    {
        settings->beginWriteArray(kUserConnectionsSectionTag);
        const auto arrayWriteGuard = QnRaiiGuard::createDestructable(
            [settings]() { settings->endArray(); });
        
        settings->remove(QString());  // Clear children
        const auto connCount = connections.size();
        for (int i = 0; i != connCount; ++i)
        {
            settings->setArrayIndex(i);
            const auto &connection = connections.at(i);
            const auto encryptedPass = xorEncrypt(connection.password, kXorKey);
            settings->setValue(kUserNameTag, connection.userName);
            settings->setValue(kPasswordTag, encryptedPass);
            settings->setValue(kSystemNameTag, connection.systemName);
            settings->setValue(kStoredPassword, connection.isStoredPassword);
        }
    }

    QnUserRecentConnectionDataList readRecentUserConnections(QSettings *settings)
    {
        QnUserRecentConnectionDataList result;
        const int count = settings->beginReadArray(kUserConnectionsSectionTag);
        const auto endArrayGuard = QnRaiiGuard::createDestructable(
            [settings]() { settings->endArray(); });

        for (int i = 0; i != count; ++i)
        {
            settings->setArrayIndex(i);

            const auto encryptedPass = settings->value(kPasswordTag).toString();

            QnUserRecentConnectionData data;
            data.userName = settings->value(kUserNameTag).toString();
            data.password = xorDecrypt(encryptedPass, kXorKey);
            data.systemName = settings->value(kSystemNameTag).toString();
            data.isStoredPassword = settings->value(kStoredPassword).toBool();
            result.append(data);
        }
        return result;
    }

}

QnCoreSettings::QnCoreSettings(QObject *parent)
    : m_settings(new QSettings(kCoreSettingsFile))
{
    init();
    updateFromSettings(m_settings.data());
}

QnCoreSettings::~QnCoreSettings()
{
    submitToSettings(m_settings.data());
}

void QnCoreSettings::writeValueToSettings(QSettings *settings
    , int id
    , const QVariant &value) const
{
    switch (id)
    {
    case RecentUserConnections:
        writeRecentUserConnections(settings
            , value.value<QnUserRecentConnectionDataList>());
        break;
    default:
        base_type::writeValueToSettings(settings, id, value);
    }
}

QVariant QnCoreSettings::readValueFromSettings(QSettings *settings
    , int id
    , const QVariant &defaultValue)
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
