
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <utils/common/singleton.h>
#include <statistics/statistics_fwd.h>
#include <api/server_rest_connection_fwd.h>


class QnStatisticsManager : public QObject
    , public Singleton<QnStatisticsManager>
{
    Q_OBJECT

    typedef QObject base_type;

public:
    QnStatisticsManager(QObject *parent = nullptr);

    virtual ~QnStatisticsManager();

public:
    bool registerStatisticsModule(const QString &alias
        ,  QnAbstractStatisticsModule *module);

    void setClientId(const QnUuid &clientID);

    void setStorage(QnAbstractStatisticsStorage *storage);

    void setSettings(QnAbstractStatisticsSettingsLoader *settings);

public:
    void sendStatistics();

    void saveCurrentStatistics();

private:
    void unregisterModule(const QString &alias);

    QnMetricsHash getMetrics() const;

private:
    typedef QPointer<QnAbstractStatisticsSettingsLoader> SettingsPtr;
    typedef QPointer<QnAbstractStatisticsStorage> StoragePtr;
    typedef QPointer<QnAbstractStatisticsModule> ModulePtr;
    typedef QHash<QString, ModulePtr> ModulesMap;

    QnUuid m_clientId;

    rest::Handle m_handle;

    SettingsPtr m_settings;
    StoragePtr m_storage;

    ModulesMap m_modules;
};

#define qnStatisticsManager QnStatisticsManager::instance()