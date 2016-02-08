
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <statistics/types.h>

class QnBaseStatisticsModule;
class QnBaseStatisticsStorage;
class QnStatisticsSettingsWatcher;
class QnBaseStatisticsSettingsLoader;

class QnStatisticsManager : public QObject
{
    Q_OBJECT

    typedef QObject base_type;

public:
    QnStatisticsManager(QObject *parent = nullptr);

    virtual ~QnStatisticsManager();

    bool registerStatisticsModule(const QString &alias
        ,  QnBaseStatisticsModule *module);

    void setStorage(QnBaseStatisticsStorage *storage);

    void setSettings(QnBaseStatisticsSettingsLoader *settings);

public:
    void sendStatistics();

    void saveCurrentStatistics();

private:
    void unregisterModule(const QString &alias);

    QnMetricsHash getMetrics() const;

    QnStringsSet getLastFilters() const;

    qint64 getLastSentTime() const;

private:
    typedef QPointer<QnBaseStatisticsSettingsLoader> SettingsPtr;
    typedef QPointer<QnBaseStatisticsStorage> StoragePtr;
    typedef QPointer<QnBaseStatisticsModule> ModulePtr;
    typedef QHash<QString, ModulePtr> ModulesMap;

    SettingsPtr m_settings;
    StoragePtr m_storage;

    ModulesMap m_modules;
    bool m_statisticsSent;
};