#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QSettings>

#include <nx/analytics/db/analytics_db_settings.h>
#include <nx/vms/server/settings.h>

class QnMediaServerModule;

/**
 * QSettings instance is initialized in \a initializeROSettingsFromConfFile or
 * \a initializeRunTimeSettingsFromConfFile call or on first demand
 */
class MSSettings: public QObject
{
    Q_OBJECT;
public:
    MSSettings(
        const QString& roSettingsPath = QString(),
        const QString& rwSettingsPath = QString());

    const nx::vms::server::Settings& settings() const;
    nx::vms::server::Settings* mutableSettings();
    QSettings* roSettings();
    const QSettings* roSettings() const;
    QSettings* runTimeSettings();
    const QSettings* runTimeSettings() const;

    void syncRoSettings() const;
    void close();

    nx::analytics::db::Settings analyticEventsStorage() const;

    static QString defaultConfigDirectory();

    static QString defaultConfigFileName;
    static QString defaultConfigFileNameRunTime;

private:
    void initializeROSettingsFromConfFile( const QString& fileName );
    void initializeROSettings();
    void initializeRunTimeSettingsFromConfFile( const QString& fileName );
    void initializeRunTimeSettings();

    void loadAnalyticEventsStorageSettings();

private:
    nx::vms::server::Settings m_settings;
    std::unique_ptr<QSettings> m_rwSettings;
    std::shared_ptr<QSettings> m_roSettings;
    nx::analytics::db::Settings m_analyticEventsStorage;
};
