#include "abstract_search_list_model.h"

#include <chrono>

#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {

AbstractSearchListModel::AbstractSearchListModel(QObject* parent):
    base_type(parent)
{
}

bool AbstractSearchListModel::canFetchMore(const QModelIndex& parent) const
{
    return parent.isValid() ? false : canFetch();
}

void AbstractSearchListModel::fetchMore(const QModelIndex& parent)
{
    if (!parent.isValid())
        requestFetch();
}

bool AbstractSearchListModel::fetchInProgress() const
{
    return false;
}

bool AbstractSearchListModel::cancelFetch()
{
    return false;
}

bool AbstractSearchListModel::isConstrained() const
{
    return m_relevantTimePeriod != QnTimePeriod::anytime();
}

const QnTimePeriod& AbstractSearchListModel::relevantTimePeriod() const
{
    return m_relevantTimePeriod;
}

void AbstractSearchListModel::setRelevantTimePeriod(const QnTimePeriod& value)
{
    if (value == m_relevantTimePeriod)
        return;

    NX_VERBOSE(this) << "Relevant time period changed"
        << "\n --- Old was from"
        << utils::timestampToDebugString(m_relevantTimePeriod.startTimeMs) << "to"
        << utils::timestampToDebugString(m_relevantTimePeriod.endTimeMs())
        << "\n --- New is from"
        << utils::timestampToDebugString(value.startTimeMs) << "to"
        << utils::timestampToDebugString(value.endTimeMs());

    m_relevantTimePeriod = value;

    if (!m_relevantTimePeriod.isValid() || !m_fetchedTimeWindow.contains(m_relevantTimePeriod))
    {
        clear();
    }
    else
    {
        cancelFetch();
        truncateToRelevantTimePeriod();
        setFetchedTimeWindow(m_relevantTimePeriod);
    }
}

AbstractSearchListModel::FetchDirection AbstractSearchListModel::fetchDirection() const
{
    return m_fetchDirection;
}

void AbstractSearchListModel::setFetchDirection(FetchDirection value)
{
    if (m_fetchDirection == value)
        return;

    NX_VERBOSE(this) << "Set fetch direction to" << value;

    cancelFetch();
    m_fetchDirection = value;
}

int AbstractSearchListModel::maximumCount() const
{
    return m_maximumCount;
}

void AbstractSearchListModel::setMaximumCount(int value)
{
    if (m_maximumCount == value)
        return;

    NX_VERBOSE(this) << "Set maximum count to" << value;

    m_maximumCount = value;

    const bool needToTruncate = m_maximumCount < rowCount();
    if (needToTruncate)
    {
        NX_VERBOSE(this) << "Truncating to maximum count";
        truncateToMaximumCount();
    }
}

int AbstractSearchListModel::fetchBatchSize() const
{
    return m_fetchBatchSize;
}

void AbstractSearchListModel::setFetchBatchSize(int value)
{
    NX_VERBOSE(this) << "Set fetch batch size to" << value;
    m_fetchBatchSize = value;
}

bool AbstractSearchListModel::live() const
{
    return m_live;
}

void AbstractSearchListModel::setLive(bool value)
{
    if (m_live == value)
        return;

    m_live = value;
    clear();
}

QnTimePeriod AbstractSearchListModel::fetchedTimeWindow() const
{
    return m_fetchedTimeWindow;
}

void AbstractSearchListModel::setFetchedTimeWindow(const QnTimePeriod& value)
{
    NX_VERBOSE(this) << "Set fetched time window from"
        << utils::timestampToDebugString(value.startTimeMs) << "to"
        << utils::timestampToDebugString(value.endTimeMs());

    if (value.endTimeMs() == DATETIME_NOW)
    {
        qDebug() << "охгдеж";
        m_fetchedTimeWindow = value;
        return;
    }

    m_fetchedTimeWindow = value;
}

void AbstractSearchListModel::finishFetch(bool cancelled)
{
    emit fetchFinished(cancelled, {});
}

void AbstractSearchListModel::clear()
{
    NX_VERBOSE(this) << "Clear model";
    setFetchDirection(FetchDirection::earlier);
    setFetchedTimeWindow({});
    cancelFetch();
    clearData();
}

} // namespace desktop
} // namespace client
} // namespace nx
