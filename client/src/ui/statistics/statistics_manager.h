
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <ui/statistics/types.h>

class QnBaseStatisticsModule;
class QnBaseStatisticsStorage;
class QnStatisticsSettingsWatcher;

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

public:
    void sendStatistics();

private:
    void unregisterModule(const QString &alias);

    QnMetricsHash getMetrics() const;

    QnStringsSet getLastFilters() const;

    qint64 getLastSentTime() const;

private:
    typedef QScopedPointer<QnStatisticsSettingsWatcher> QnStatisticsSettingsWatcherPtr;
    typedef QPointer<QnBaseStatisticsStorage> StoragePtr;
    typedef QPointer<QnBaseStatisticsModule> ModulePtr;
    typedef QHash<QString, ModulePtr> ModulesMap;

    const QnStatisticsSettingsWatcherPtr m_settings;
    ModulesMap m_modules;
    StoragePtr m_storage;
};