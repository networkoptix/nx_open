// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include "statistics_manager.h"

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QTimer>

#include <api/global_settings.h>
#include <api/helpers/send_statistics_request_data.h>
#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/statistics/statistics_settings_watcher.h>
#include <statistics/abstract_statistics_module.h>
#include <statistics/abstract_statistics_storage.h>

using namespace nx::vms::client::desktop;

namespace
{
    const auto kFiltersSettings = lit("filters");
    const auto kLastSentTime = lit("lastSentTime");

    template<typename ValueType>
    void saveToStorage(const QString &name
        , const ValueType &value
        , const QnStatisticsStoragePtr &storage)
    {
        NX_ASSERT(!name.isEmpty(), "Name could not be empty!");
        NX_ASSERT(storage, "Storage has not set!");

        if (!storage || name.isEmpty())
            return;

        const auto json = QJson::serialized(value);
        storage->saveCustomSettings(name, json);
    }

    void saveLastFilters(const QnStringsSet &filters
        , const QnStatisticsStoragePtr &storage)
    {
        saveToStorage(kFiltersSettings, filters, storage);
    }

    void saveLastSentTime(qint64 lastSentTime
        , const QnStatisticsStoragePtr &storage)
    {
        saveToStorage(kLastSentTime, lastSentTime, storage);
    }

    QnStringsSet getLastFilters(const QnStatisticsStoragePtr &storage)
    {
        NX_ASSERT(storage, "Storage has not set!");

        if (!storage)
            return QnStringsSet();

        const auto jsonValue = storage->getCustomSettings(kFiltersSettings);
        return QJson::deserialized<QnStringsSet>(jsonValue);
    }


    qint64 getLastSentTime(const QnStatisticsStoragePtr &storage)
    {
        NX_ASSERT(storage, "Storage has not set!");

        enum { kMinLastTimeMs = 0 };
        if (!storage)
            return kMinLastTimeMs;

        const auto lastSentTimeStr = storage->getCustomSettings(kLastSentTime);
        return QJson::deserialized<qint64>(lastSentTimeStr, kMinLastTimeMs);
    }

    //

    bool isMatchFilters(const QString &fullAlias
        , const QnStringsSet &filters)
    {
        const auto matchFilter = [fullAlias](const QString &filter)
        {
            return QRegExp(filter).exactMatch(fullAlias);
        };

        return std::any_of(filters.cbegin(), filters.cend(), matchFilter);
    }

    QnStatisticValuesHash filteredMetrics(const QnStatisticValuesHash &source
        , const QnStringsSet &filters)
    {
        QnStatisticValuesHash result;
        for (auto it = source.cbegin(); it != source.cend(); ++it)
        {
            const auto metricAlias = it.key();
            const auto metricValue = it.value();
            if (isMatchFilters(metricAlias, filters))
                result.insert(metricAlias, metricValue);
        }
        return result;
    }

    const auto kSessionIdMetricTag = lit("id");
    const auto kClientMachineIdMetricTag = lit("clientId");
    const auto kSystemNameMetricTag = lit("systemName");
    const auto kCloudSystemIdMetricTag = lit("cloudSystemId");

    void appendMandatoryFilters(QnStringsSet *filters)
    {
        if (!filters)
            return;

        static const QString kKeyfilters[] =
        {
            kSessionIdMetricTag,
            kClientMachineIdMetricTag,
            kSystemNameMetricTag,
            kCloudSystemIdMetricTag,
        };

        for (auto keyFilter: kKeyfilters)
            filters->insert(keyFilter);
    }

} // namespace

QnStatisticsManager::QnStatisticsManager(QObject *parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    m_updateSettingsTimer(new QTimer()),
    m_clientId(),
    m_sessionId(QnUuid::createUuid()),
    m_handle(),
    m_settings(new StatisticsSettingsWatcher()),
    m_storage(),
    m_modules()
{
    enum { kUpdatePeriodMs = 4 * 60 * 60 * 1000 };  // every 4 hours
    m_updateSettingsTimer->setSingleShot(false);
    m_updateSettingsTimer->setInterval(kUpdatePeriodMs);
    m_updateSettingsTimer->callOnTimeout(
        [this]()
        {
            if (isStatisticsSendingAllowed())
                m_settings->updateSettings();
        });
    m_updateSettingsTimer->start();

    connect(m_settings.get(), &StatisticsSettingsWatcher::settingsAvailableChanged, this,
        [this]()
        {
            if (m_settings->settings())
                sendStatistics();
        });
}

QnStatisticsManager::~QnStatisticsManager()
{}

bool QnStatisticsManager::registerStatisticsModule(const QString &alias
    ,  QnAbstractStatisticsModule *module)
{
    if (m_modules.contains(alias))
        return false;

    m_modules.insert(alias, ModulePtr(module));

    QPointer<QnStatisticsManager> thisSafePtr = this;
    const auto unregisterModuleHandler = [alias, thisSafePtr]()
    {
        if (thisSafePtr)
            thisSafePtr->unregisterModule(alias);
    };

    connect(module, &QObject::destroyed, this, unregisterModuleHandler);
    return true;
}

bool QnStatisticsManager::isStatisticsSendingAllowed() const
{
    // Exclude developers from the statisics.
    if (nx::build_info::publicationType() == nx::build_info::PublicationType::local)
        return false;

    return qnGlobalSettings->isInitialized()
        && qnGlobalSettings->isStatisticsAllowed()
        && !qnGlobalSettings->isNewSystem();
}

void QnStatisticsManager::unregisterModule(const QString &alias)
{
    m_modules.remove(alias);
}

void QnStatisticsManager::setClientId(const QnUuid &clientID)
{
    m_clientId = clientID;
}

void QnStatisticsManager::setStorage(QnStatisticsStoragePtr storage)
{
    m_storage = std::move(storage);
}

QnStatisticValuesHash QnStatisticsManager::getValues() const
{
    QnStatisticValuesHash result;
    for (auto it = m_modules.cbegin(); it != m_modules.cend(); ++it)
    {
        const auto moduleAlias = it.key();
        const auto module = it.value();
        if (!module)
            continue;

        const auto values = module->values();
        for (auto itValue = values.cbegin(); itValue != values.cend(); ++itValue)
        {
            const auto subAlias = itValue.key();
            const auto fullAlias = lit("%1_%2").arg(moduleAlias, subAlias);
            result.insert(fullAlias, itValue.value());
        }
    }
    return result;
}

void QnStatisticsManager::sendStatistics()
{
    {
        QMutexLocker lock(&m_mutex);
        if (!m_settings || !m_storage || m_handle || !isStatisticsSendingAllowed())
            return;
    }

    if (!m_settings->settings())
    {
        m_settings->updateSettings();
        return;
    }

    auto server = commonModule()->currentServer();
    if (!server || !server->hasInternetAccess())
        server = resourcePool()->serverWithInternetAccess();

    if (!server || !connection())
        return;

    QnStatisticsSettings settings = *m_settings->settings();
    if (settings.storeDays < 0)
        settings.storeDays = 0;

    appendMandatoryFilters(&settings.filters);

    static const qint64 kMsInDay = 24 * 60 * 60 * 1000;
    const auto timeStamp = QDateTime::currentMSecsSinceEpoch();
    const bool filtersChanged = (getLastFilters(m_storage) != settings.filters);
    const qint64 minTimeStampMs = (filtersChanged
        ? timeStamp - kMsInDay * settings.storeDays
        : getLastSentTime(m_storage));

    const auto msSinceSent = (timeStamp > minTimeStampMs
        ? timeStamp - minTimeStampMs : 0);

    enum { kMsInSec = 1000 };
    if (!filtersChanged && (msSinceSent < settings.minSendPeriodSecs * kMsInSec))
        return;

    const auto totalMetricsList = m_storage->getMetricsList(
        minTimeStampMs, settings.limit);

    const auto sendStatisticsCallback = nx::utils::guarded(this,
        [this, timeStamp, settings](bool success,
            rest::Handle handle,
            rest::ServerConnection::EmptyResponseType /*response */)
        {
            if (m_handle != handle)
                return;

            {
                QMutexLocker lock(&m_mutex);
                m_handle = rest::Handle();
            }

            if (!success || !m_storage)
                return;

            saveLastSentTime(timeStamp, m_storage);
            saveLastFilters(settings.filters, m_storage);
        });

    const auto remoteGuid = commonModule()->remoteGUID();
    // Metrics filtration may takes significant time, so run it in an another thread.
    const auto sendFilteredMetrics =
        [this, server, totalMetricsList, settings, timeStamp, sendStatisticsCallback, remoteGuid]
        {
            QnMetricHashesList totalFiltered;
            for (const auto metrics: totalMetricsList)
            {
                const auto filtered = filteredMetrics(metrics, settings.filters);
                if (!filtered.isEmpty())
                    totalFiltered.push_back(filtered);
            }

            if (totalFiltered.empty())
                return;

            QnSendStatisticsRequestData statisticsData;
            statisticsData.statisticsServerUrl = settings.statisticsServerUrl;
            statisticsData.metricsList = totalFiltered;
            statisticsData.format = Qn::SerializationFormat::JsonFormat;

            // Check if system not changed.
            if (remoteGuid == commonModule()->remoteGUID())
            {
                QMutexLocker lock(&m_mutex);
                m_handle = connectedServerApi()->sendStatisticsUsingServer(
                    server->getId(),
                    statisticsData,
                    sendStatisticsCallback,
                    thread());
            }
        };

    QtConcurrent::run(sendFilteredMetrics);
}

void QnStatisticsManager::saveCurrentStatistics()
{
    NX_ASSERT(!m_clientId.isNull()
        , "Can't save client statistics without client identifier!");

    if (m_clientId.isNull())
        return;

    auto metrics = getValues();

    if (metrics.empty())
        return;

    // Appends mandatory metrics

    const auto systemName = qnGlobalSettings->systemName();
    const auto cloudSystemId = qnGlobalSettings->cloudSystemId();
    metrics.insert(kSessionIdMetricTag, m_sessionId.toString());
    metrics.insert(kClientMachineIdMetricTag, m_clientId.toString());

    m_storage->storeMetrics(metrics);
}

void QnStatisticsManager::resetStatistics()
{
    m_sessionId = QnUuid::createUuid();
    for(const auto module: m_modules)
    {
        if (module)
            module->reset();
    }
}
