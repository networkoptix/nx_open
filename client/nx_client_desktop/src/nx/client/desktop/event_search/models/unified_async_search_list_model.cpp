#include "unified_async_search_list_model.h"

#include <ui/models/sort_filter_list_model.h>
#include <utils/common/delayed.h>

#include <nx/client/desktop/common/models/subset_list_model.h>
#include <nx/client/desktop/event_search/models/abstract_async_search_list_model.h>
#include <nx/client/desktop/event_search/models/private/busy_indicator_model_p.h>

namespace nx {
namespace client {
namespace desktop {

UnifiedAsyncSearchListModel::UnifiedAsyncSearchListModel(
    AbstractAsyncSearchListModel* sourceModel,
    QObject* parent)
    :
    base_type(parent),
    m_sourceModel(sourceModel),
    m_filterModel(new QnSortFilterListModel(this)),
    m_busyIndicatorModel(new BusyIndicatorModel(this))
{
    NX_ASSERT(sourceModel);
    setModels({new SubsetListModel(m_filterModel, 0, QModelIndex(), this), m_busyIndicatorModel});

    m_filterModel->setSourceModel(m_sourceModel);
    m_filterModel->setFilterRole(Qn::ResourceSearchStringRole);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

UnifiedAsyncSearchListModel::~UnifiedAsyncSearchListModel()
{
}

QString UnifiedAsyncSearchListModel::textFilter() const
{
    return m_filterModel->filterWildcard();
}

void UnifiedAsyncSearchListModel::setTextFilter(const QString& value)
{
    m_filterModel->setFilterWildcard(value);
}

QnTimePeriod UnifiedAsyncSearchListModel::selectedTimePeriod() const
{
    return m_sourceModel->selectedTimePeriod();
}

void UnifiedAsyncSearchListModel::setSelectedTimePeriod(const QnTimePeriod& value)
{
    m_sourceModel->setSelectedTimePeriod(value);
}

bool UnifiedAsyncSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return m_sourceModel && m_sourceModel->canFetchMore();
}

void UnifiedAsyncSearchListModel::fetchMore(const QModelIndex& /*parent*/)
{
    if (!canFetchMore())
        return;

    auto prefetchCompletionHandler =
        [this, guard = QPointer<UnifiedAsyncSearchListModel>(this)](qint64 earliestTimeMs)
        {
            if (!guard)
                return;

            m_busyIndicatorModel->setActive(false);

            if (earliestTimeMs < 0) //< Was cancelled.
                return;

            const int previousRowCount = m_filterModel->rowCount();
            m_sourceModel->commitPrefetch(earliestTimeMs);

            const bool hasClientSideFilter = !textFilter().isEmpty();
            if (!hasClientSideFilter)
                return;

            // If fetch produced enough records after filtering, do nothing.
            const auto kReFetchCountThreshold = 15;
            if (m_filterModel->rowCount() - previousRowCount > kReFetchCountThreshold)
                return;

            // Queue more fetch after a short delay.
            static const int kReFetchDelayMs = 50;
            executeDelayedParented([this]() { fetchMore(); }, kReFetchDelayMs, this);
        };

    const auto fetchInProgress = m_sourceModel->prefetchAsync(prefetchCompletionHandler);
    m_busyIndicatorModel->setActive(fetchInProgress);
}

} // namespace desktop
} // namespace client
} // namespace nx
