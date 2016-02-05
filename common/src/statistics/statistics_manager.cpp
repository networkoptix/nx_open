
#include "statistics_manager.h"

#include <common/common_module.h>
#include <utils/common/model_functions.h>
#include <api/media_server_connection.h>
#include <core/resource/media_server_resource.h>

#include <statistics/base_statistics_module.h>
#include <statistics/base_statistics_storage.h>
#include <statistics/base_statistics_settings_loader.h>

namespace
{
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
}

QnStatisticsManager::QnStatisticsManager(QObject *parent)
    : base_type(parent)
    , m_settings()
    , m_modules()
    , m_storage()
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

void QnStatisticsManager::setStorage(QnBaseStatisticsStorage *storage)
{
    m_storage = storage;
}

void QnStatisticsManager::setSettings(QnBaseStatisticsSettingsLoader *settings)
{
    m_settings = settings;
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
    for(auto it = metrics.begin(); it != metrics.end(); ++it)
    {
        qDebug() << it.key() << " : " << it.value();
    }

}

void QnStatisticsManager::sendStatistics()
{
//    if (!qnGlobalSettings->isStatisticsAllowed()) // TODO: uncomment me!
//        return;

    //
    static const qint64 kMsInDay = 24 * 60 * 60 * 1000;
    auto settings = m_settings->settings();
    settings.filters.insert(lit(".*"));    // TODO: remove me, add const to settings
    const qint64 minTimeStampMs = (getLastFilters() == settings.filters
        ? QDateTime::currentMSecsSinceEpoch() - kMsInDay * settings.storeDays
        : getLastSentTime());

    const auto totalMetricsList = m_storage->getMetricsList(minTimeStampMs, settings.limit);

    QnMetricHashesList totalFiltered;
    for (const auto metrics: totalMetricsList)
    {
        totalFiltered.push_back(filteredMetrics(
            metrics, m_settings->settings().filters));
    }

    const auto server = qnCommon->currentServer();
    const auto connection = server->apiConnection();

}

void QnStatisticsManager::saveCurrentStatistics()
{
    // Stores current metrics
    const auto metrics = getMetrics();
    m_storage->storeMetrics(metrics);

    // TODO: remove me
    const auto settings = m_settings->settings();
    debugOutput(filteredMetrics(metrics, settings.filters));
}

QnStringsSet QnStatisticsManager::getLastFilters() const
{
    static const auto kFiltersSettings = lit("filters");

    const auto filterJsonSettings = m_storage->getCustomSettings(kFiltersSettings);
    return QJson::deserialized<QnStringsSet>(filterJsonSettings);
}

qint64 QnStatisticsManager::getLastSentTime() const
{
    enum { kMinLastSentTime = 0 };
    static const auto kLastSentTime = lit("lastSentTime");

    const auto lastSentTimeStr = m_storage->getCustomSettings(kLastSentTime);
    return QnLexical::deserialized<qint64>(QString::fromLatin1(lastSentTimeStr)
        , kMinLastSentTime);
}




