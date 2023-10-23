// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "statistics_manager.h"

#include <nx/fusion/model_functions.h>
#include <statistics/abstract_statistics_module.h>
#include <statistics/abstract_statistics_storage.h>

namespace {

static const QString kFiltersSettings("filters");
static const QString kLastSentTime("lastSentTime");
static const QString kSessionIdMetricTag("id");
static const QString kClientMachineIdMetricTag("clientId");

template<typename ValueType>
void saveToStorage(
    const QString& name, const ValueType& value, const QnStatisticsStoragePtr& storage)
{
    NX_ASSERT(!name.isEmpty(), "Name could not be empty!");
    NX_ASSERT(storage, "Storage has not set!");

    if (!storage || name.isEmpty())
        return;

    const auto json = QJson::serialized(value);
    storage->saveCustomSettings(name, json);
}

QnStringsSet getLastFilters(const QnStatisticsStoragePtr& storage)
{
    NX_ASSERT(storage, "Storage has not set!");

    if (!storage)
        return QnStringsSet();

    const auto jsonValue = storage->getCustomSettings(kFiltersSettings);
    return QJson::deserialized<QnStringsSet>(jsonValue);
}

qint64 getLastSentTime(const QnStatisticsStoragePtr& storage)
{
    NX_ASSERT(storage, "Storage has not set!");

    constexpr auto kMinLastTimeMs = 0;

    if (!storage)
        return kMinLastTimeMs;

    const auto lastSentTimeStr = storage->getCustomSettings(kLastSentTime);
    return QJson::deserialized<qint64>(lastSentTimeStr, kMinLastTimeMs);
}

bool isMatchFilters(const QString& fullAlias, const QnStringsSet& filters)
{
    const auto matchFilter =
        [fullAlias](const QString& filter)
        {
            const QRegularExpression rx(QRegularExpression::anchoredPattern(filter));
            return rx.match(fullAlias).hasMatch();
        };

    return std::any_of(filters.cbegin(), filters.cend(), matchFilter);
}

QnStatisticValuesHash filteredMetrics(
    const QnStatisticValuesHash& source,
    const QnStringsSet& filters)
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

void appendMandatoryFilters(QnStringsSet* filters)
{
    if (!filters)
        return;

    static const QString kKeyFilters[] = {
        kSessionIdMetricTag,
        kClientMachineIdMetricTag,
    };

    for (auto keyFilter: kKeyFilters)
        filters->insert(keyFilter);
}

} // namespace

QnStatisticsManager::QnStatisticsManager(QObject* parent):
    base_type(parent),
    m_sessionId(QnUuid::createUuid())
{
}

QnStatisticsManager::~QnStatisticsManager()
{
}

bool QnStatisticsManager::registerStatisticsModule(
    const QString& alias,
    QnAbstractStatisticsModule* module)
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

void QnStatisticsManager::unregisterModule(const QString& alias)
{
    m_modules.remove(alias);
}

void QnStatisticsManager::setClientId(const QnUuid& clientID)
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

std::optional<QnStatisticsManager::StatisticsData> QnStatisticsManager::prepareStatisticsData(
    QnStatisticsSettings settings) const
{
    if (!NX_ASSERT(m_storage))
        return {};

    if (settings.storeDays < 0)
        settings.storeDays = 0;

    appendMandatoryFilters(&settings.filters);

    constexpr qint64 kMsInDay = 24 * 60 * 60 * 1000;
    // Always operate with statistics in local system time.
    const auto timestamp = QDateTime::currentMSecsSinceEpoch();
    const bool filtersChanged = (getLastFilters(m_storage) != settings.filters);
    const qint64 minTimeStampMs = filtersChanged
        ? timestamp - kMsInDay * settings.storeDays
        : getLastSentTime(m_storage);

    const auto msSinceSent = (timestamp > minTimeStampMs ? timestamp - minTimeStampMs : 0);

    constexpr auto kMsInSec = 1000;

    if (!filtersChanged && (msSinceSent < settings.minSendPeriodSecs * kMsInSec))
        return {};

    const auto totalMetricsList = m_storage->getMetricsList(minTimeStampMs, settings.limit);

    QnMetricHashesList totalFiltered;
    for (const auto metrics: totalMetricsList)
    {
        const auto filtered = filteredMetrics(metrics, settings.filters);
        if (!filtered.isEmpty())
            totalFiltered.push_back(filtered);
    }

    if (totalFiltered.empty())
        return {};

    StatisticsData result;
    result.payload.statisticsServerUrl = settings.statisticsServerUrl;
    result.payload.metricsList = totalFiltered;
    result.payload.format = Qn::SerializationFormat::json;
    result.filters = settings.filters;
    result.timestamp = timestamp;
    return result;
}

void QnStatisticsManager::saveSentStatisticsData(qint64 timestamp, const QnStringsSet& filters)
{
    if (!NX_ASSERT(m_storage))
        return;

    saveToStorage(kLastSentTime, timestamp, m_storage);
    saveToStorage(kFiltersSettings, filters, m_storage);
}

void QnStatisticsManager::saveCurrentStatistics()
{
    NX_ASSERT(!m_clientId.isNull(), "Can't save client statistics without client identifier!");

    if (m_clientId.isNull())
        return;

    auto metrics = getValues();

    if (metrics.empty())
        return;

    // Appends mandatory metrics
    metrics.insert(kSessionIdMetricTag, m_sessionId.toString());
    metrics.insert(kClientMachineIdMetricTag, m_clientId.toString());

    m_storage->storeMetrics(metrics);
}

void QnStatisticsManager::resetStatistics()
{
    m_sessionId = QnUuid::createUuid();
    for (const auto module: m_modules)
    {
        if (module)
            module->reset();
    }
}
