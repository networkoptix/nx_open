
#include "statistics_manager.h"

#include <utils/common/model_functions.h>
#include <common/common_module.h>
#include <api/server_rest_connection.h>
#include <api/helpers/send_statistics_request_data.h>
#include <core/resource/media_server_resource.h>

#include <statistics/base_statistics_module.h>
#include <statistics/base_statistics_storage.h>
#include <statistics/base_statistics_settings_loader.h>

namespace
{
    const auto kFiltersSettings = lit("filters");
    const auto kLastSentTime = lit("lastSentTime");

    template<typename ValueType>
    void saveToStorage(const QString &name
        , const ValueType &value
        , QnBaseStatisticsStorage *storage)
    {
        Q_ASSERT_X(!name.isEmpty(), Q_FUNC_INFO, "Name could not be empty!");
        Q_ASSERT_X(storage, Q_FUNC_INFO, "Storage has not set!");

        if (!storage || name.isEmpty())
            return;

        const auto json = QJson::serialized(value);
        storage->saveCustomSettings(name, json);
    }

    void saveLastFilters(const QnStringsSet &filters
        , QnBaseStatisticsStorage *storage)
    {
        saveToStorage(kFiltersSettings, filters, storage);
    }

    void saveLastSentTime(qint64 lastSentTime
        , QnBaseStatisticsStorage *storage)
    {
        saveToStorage(kLastSentTime, lastSentTime, storage);
    }

    QnStringsSet getLastFilters(const QnBaseStatisticsStorage *storage)
    {
        Q_ASSERT_X(storage, Q_FUNC_INFO, "Storage has not set!");

        if (!storage)
            return QnStringsSet();

        const auto jsonValue = storage->getCustomSettings(kFiltersSettings);
        return QJson::deserialized<QnStringsSet>(jsonValue);
    }


    qint64 getLastSentTime(const QnBaseStatisticsStorage *storage)
    {
        Q_ASSERT_X(storage, Q_FUNC_INFO, "Storage has not set!");

        enum { kMinLastTimeMs = 0 };
        if (!storage)
            return kMinLastTimeMs;

        const auto lastSentTimeStr = storage->getCustomSettings(kFiltersSettings);
        return QnLexical::deserialized<qint64>(
            QString::fromLatin1(lastSentTimeStr, kMinLastTimeMs));
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

    QnMetricsHash filteredMetrics(const QnMetricsHash &source
        , const QnStringsSet &filters)
    {
        QnMetricsHash result;
        for (auto it = source.cbegin(); it != source.cend(); ++it)
        {
            if (isMatchFilters(it.key(), filters))
                result.insert(it.key(), it.value());
        }
        return result;
    }

    const auto kSessionIdMetricTag = lit("id");
    const auto kClientMachineIdMetricTag = lit("clientId");
    const auto kSystemNameMetricTag = lit("systemName");

    void appendMandatoryFilters(QnStringsSet *filters)
    {
        if (!filters)
            return;

        static const QString kKeyfilters[] = { kSessionIdMetricTag
            , kClientMachineIdMetricTag, kSystemNameMetricTag };

        for (auto keyFilter: kKeyfilters)
            filters->insert(keyFilter);
    }
}

QnStatisticsManager::QnStatisticsManager(QObject *parent)
    : base_type(parent)
    , m_clientId()
    , m_connection()
    , m_settings()
    , m_modules()
    , m_storage()
    , m_statisticsSent(false)
{}

QnStatisticsManager::~QnStatisticsManager()
{}

bool QnStatisticsManager::registerStatisticsModule(const QString &alias
    ,  QnBaseStatisticsModule *module)
{
    if (m_modules.contains(alias))
    {
        Q_ASSERT_X(false, Q_FUNC_INFO, "Module has been registered already");
        return false;
    }

    m_modules.insert(alias, ModulePtr(module));

    QPointer<QnStatisticsManager> thisSafePtr = this;
    const auto unregisterModuleHandler = [this, alias, thisSafePtr]()
    {
        if (thisSafePtr)
            thisSafePtr->unregisterModule(alias);
    };

    connect(module, &QObject::destroyed, this, unregisterModuleHandler);
    return true;
}

void QnStatisticsManager::unregisterModule(const QString &alias)
{
    m_modules.remove(alias);
}

void QnStatisticsManager::setClientId(const QnUuid &clientID)
{
    m_clientId = clientID;
}

void QnStatisticsManager::setStorage(QnBaseStatisticsStorage *storage)
{
    m_storage = storage;
}

void QnStatisticsManager::setSettings(QnBaseStatisticsSettingsLoader *settings)
{
    if (m_settings)
        disconnect(m_settings, nullptr, this, nullptr);

    m_settings = settings;

    if (m_settings->settingsAvailable())
        sendStatistics();

    connect(m_settings, &QnBaseStatisticsSettingsLoader::settingsAvailableChanged
        , this, &QnStatisticsManager::sendStatistics);
}

QnMetricsHash QnStatisticsManager::getMetrics() const
{
    QnMetricsHash result;
    for (auto it = m_modules.cbegin(); it != m_modules.cend(); ++it)
    {
        const auto moduleAlias = it.key();
        const auto data = it.value()->metrics();
        for (auto itData = data.cbegin(); itData != data.cend(); ++itData)
        {
            const auto subAlias = itData.key();
            const auto fullAlias = lit("%1_%2").arg(moduleAlias, subAlias);
            result.insert(fullAlias, itData.value());
        }
    }
    return result;
}

void debugOutput(const QnMetricsHash &metrics)
{
    qDebug() << "Stat:";
    for(auto it = metrics.begin(); it != metrics.end(); ++it)
    {
        qDebug() << it.key() << " : " << it.value();
    }
}

void QnStatisticsManager::sendStatistics()
{
    if (!m_settings || m_connection)
        return;

//    if (!qnGlobalSettings->isStatisticsAllowed()) // TODO: uncomment me!
//        return;

    m_statisticsSent = false;
    if (!m_settings->settingsAvailable())
        return;

    QnStatisticsSettings settings = m_settings->settings();
    appendMandatoryFilters(&settings.filters);

    static const qint64 kMsInDay = 24 * 60 * 60 * 1000;
    const qint64 minTimeStampMs = (getLastFilters(m_storage) == settings.filters
        ? QDateTime::currentMSecsSinceEpoch() - kMsInDay * settings.storeDays
        : getLastSentTime(m_storage));

    const auto totalMetricsList = m_storage->getMetricsList(minTimeStampMs, settings.limit);

    QnMetricHashesList totalFiltered;
    for (const auto metrics: totalMetricsList)
    {
        const auto filtered = filteredMetrics(metrics, settings.filters);
        if (!filtered.isEmpty())
            totalFiltered.push_back(filtered);
    }

    const auto server = qnCommon->currentServer();
    if (!server)
        return;

    m_connection = server->restConnection();

    QnSendStatisticsRequestData request;
    request.metricsList = totalFiltered;

    const auto timeStamp = QDateTime::currentMSecsSinceEpoch();
    const auto callback = [this, timeStamp, settings]
        (bool success, rest::Handle /* handle */
        , rest::ServerConnection::EmptyResponseType /*response */)
    {
        const auto connectionLifeHolder = m_connection;
        m_connection.reset();

        if (!success)
            return;

        saveLastSentTime(timeStamp, m_storage);
        saveLastFilters(settings.filters, m_storage);
        m_statisticsSent = true;
    };

    m_connection->sendStatisticsAsync(request, callback);
}

void QnStatisticsManager::saveCurrentStatistics()
{
    Q_ASSERT_X(!m_clientId.isNull(), Q_FUNC_INFO
        , "Can't save client statistics without client identifier!");
    if (m_clientId.isNull())
        return;

    // Stores current metrics
    auto metrics = getMetrics();

    const auto sessionId = qnCommon->runningInstanceGUID().toString();
    const auto systemName = qnCommon->localSystemName();
    metrics.insert(kSessionIdMetricTag, sessionId);
    metrics.insert(kClientMachineIdMetricTag, m_clientId.toString());
    metrics.insert(kSystemNameMetricTag, systemName);
    m_storage->storeMetrics(metrics);
}




