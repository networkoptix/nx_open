#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <common/common_module_aware.h>

#include <utils/common/connective.h>
#include <statistics/statistics_fwd.h>
#include <api/server_rest_connection_fwd.h>

#include <nx/utils/uuid.h>

class QnStatisticsManager: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;
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
    void setSettings(QnStatisticsLoaderPtr settings);

public:
    void sendStatistics();

    void saveCurrentStatistics();

    void resetStatistics();

private:
    bool isStatisticsSendingAllowed() const;

    void unregisterModule(const QString &alias);

    QnStatisticValuesHash getValues() const;

private:
    typedef QPointer<QnAbstractStatisticsModule> ModulePtr;
    typedef QHash<QString, ModulePtr> ModulesMap;
    typedef QScopedPointer<QTimer> TimerPtr;

    TimerPtr m_updateSettingsTimer;

    QnUuid m_clientId;

    rest::Handle m_handle;

    QnStatisticsLoaderPtr m_settings;
    QnStatisticsStoragePtr m_storage;

    ModulesMap m_modules;
};
