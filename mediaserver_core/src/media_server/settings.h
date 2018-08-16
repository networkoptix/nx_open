#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QSettings>

#include <analytics/detected_objects_storage/analytics_events_storage_settings.h>
#include <nx/mediaserver/settings.h>

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

    const nx::mediaserver::Settings& settings() const;
    nx::mediaserver::Settings* mutableSettings();
    QSettings* roSettings();
    const QSettings* roSettings() const;
    QSettings* runTimeSettings();
    const QSettings* runTimeSettings() const;

    void syncRoSettings() const;
    void close();

    nx::analytics::storage::Settings analyticEventsStorage() const;

    static QString defaultROSettingsFilePath();
    static QString defaultRunTimeSettingsFilePath();
    static QString defaultConfigDirectory();

private:
    void initializeROSettingsFromConfFile( const QString& fileName );
    void initializeROSettings();
    void initializeRunTimeSettingsFromConfFile( const QString& fileName );
    void initializeRunTimeSettings();

    void loadAnalyticEventsStorageSettings();

private:
    nx::mediaserver::Settings m_settings;
    std::unique_ptr<QSettings> m_rwSettings;
    std::shared_ptr<QSettings> m_roSettings;
    nx::analytics::storage::Settings m_analyticEventsStorage;
};
