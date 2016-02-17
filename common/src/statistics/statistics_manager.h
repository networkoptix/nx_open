
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <utils/common/singleton.h>
#include <utils/common/connective.h>
#include <statistics/statistics_fwd.h>
#include <api/server_rest_connection_fwd.h>

class QnStatisticsManager : public Connective<QObject>
    , public Singleton<QnStatisticsManager>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnStatisticsManager(QObject *parent = nullptr);

    virtual ~QnStatisticsManager();

public:
    bool registerStatisticsModule(const QString &alias
        ,  QnAbstractStatisticsModule *module);

    void setClientId(const QnUuid &clientID);

    /*!
     *  Sets storage. Note: it will takes ownership under storage object
     */
    void setStorage(QnStatisticsStoragePtr storage);

    /*!
     *  Sets settings source. Note: it will takes ownership under settings object
     */
    void setSettings(QnStatisticsSettingsPtr settings);

public:
    void sendStatistics();

    void saveCurrentStatistics();

    void resetStatistics();

private:
    void unregisterModule(const QString &alias);

    QnStatisticValuesHash getValues() const;

private:
    typedef QPointer<QnAbstractStatisticsModule> ModulePtr;
    typedef QHash<QString, ModulePtr> ModulesMap;

    QnUuid m_clientId;

    rest::Handle m_handle;

    QnStatisticsSettingsPtr m_settings;
    QnStatisticsStoragePtr m_storage;

    ModulesMap m_modules;
};

#define qnStatisticsManager QnStatisticsManager::instance()