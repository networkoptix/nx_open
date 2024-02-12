// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <api/helpers/send_statistics_request_data.h>
#include <nx/utils/uuid.h>
#include <statistics/statistics_settings.h>

/**
 * Stores separate statistics modules, aggregates and filters information.
 */
class QnStatisticsManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnStatisticsManager(QObject* parent = nullptr);
    virtual ~QnStatisticsManager();

public:
    bool registerStatisticsModule(const QString& alias, QnAbstractStatisticsModule* module);

    void setClientId(const nx::Uuid& clientID);

    void setStorage(QnStatisticsStoragePtr storage);

    struct StatisticsData
    {
        QnSendStatisticsRequestData payload;
        qint64 timestamp = 0;
        QnStringsSet filters;
    };
    std::optional<StatisticsData> prepareStatisticsData(QnStatisticsSettings settings) const;

    void saveSentStatisticsData(qint64 timestamp, const QnStringsSet& filters);

    void saveCurrentStatistics();

    void resetStatistics();

private:
    void unregisterModule(const QString& alias);

    QnStatisticValuesHash getValues() const;

private:
    typedef QPointer<QnAbstractStatisticsModule> ModulePtr;
    typedef QHash<QString, ModulePtr> ModulesMap;

    nx::Uuid m_clientId;
    nx::Uuid m_sessionId;

    QnStatisticsStoragePtr m_storage;

    ModulesMap m_modules;
};
