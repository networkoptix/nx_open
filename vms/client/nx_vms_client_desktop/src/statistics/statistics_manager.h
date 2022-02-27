// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QPointer>

#include <api/server_rest_connection_fwd.h>
#include <common/common_module_aware.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <statistics/statistics_fwd.h>
#include <utils/common/connective.h>

class QTimer;

namespace nx::vms::client::desktop { class StatisticsSettingsWatcher; }

class QnStatisticsManager:
    public Connective<QObject>,
    public QnCommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
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
    QnUuid m_sessionId;

    mutable QMutex m_mutex;
    rest::Handle m_handle;

    std::unique_ptr<nx::vms::client::desktop::StatisticsSettingsWatcher> m_settings;
    QnStatisticsStoragePtr m_storage;

    ModulesMap m_modules;
};
