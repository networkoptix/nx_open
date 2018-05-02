#include "unified_async_search_list_model.h"

#include <ui/models/sort_filter_list_model.h>
#include <utils/common/delayed.h>

#include <nx/client/desktop/common/models/subset_list_model.h>
#include <nx/client/desktop/event_search/models/abstract_event_list_model.h>
#include <nx/client/desktop/event_search/models/private/busy_indicator_model_p.h>

namespace nx {
namespace client {
namespace desktop {

UnifiedAsyncSearchListModel::UnifiedAsyncSearchListModel(
    AbstractEventListModel* sourceModel,
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

    connect(m_sourceModel, &AbstractEventListModel::fetchAboutToBeFinished, this,
        [this]()
        {
            m_busyIndicatorModel->setActive(false);
            m_previousRowCount = m_filterModel->rowCount();
        });

    connect(m_sourceModel, &AbstractEventListModel::fetchFinished, this,
        [this](bool cancelled)
        {
            const bool hasClientSideFilter = !clientsideTextFilter().isEmpty();
            if (cancelled || !hasClientSideFilter)
                return;

            // If fetch produced enough records after filtering, do nothing.
            const auto kReFetchCountThreshold = 15;
            if (m_filterModel->rowCount() - m_previousRowCount > kReFetchCountThreshold)
                return;

            // Queue more fetch after a short delay.
            static const int kReFetchDelayMs = 50;
            executeDelayedParented([this]() { fetchMore(); }, kReFetchDelayMs, this);
        });
}

UnifiedAsyncSearchListModel::~UnifiedAsyncSearchListModel()
{
}

QString UnifiedAsyncSearchListModel::clientsideTextFilter() const
{
    return m_filterModel->filterWildcard();
}

void UnifiedAsyncSearchListModel::setClientsideTextFilter(const QString& value)
{
    m_filterModel->setFilterWildcard(value);
}

QnTimePeriod UnifiedAsyncSearchListModel::relevantTimePeriod() const
{
    return m_sourceModel->relevantTimePeriod();
}

void UnifiedAsyncSearchListModel::setRelevantTimePeriod(const QnTimePeriod& value)
{
    m_sourceModel->setRelevantTimePeriod(value);
}

bool UnifiedAsyncSearchListModel::isConstrained() const
{
    return m_sourceModel && (!clientsideTextFilter().isEmpty() || m_sourceModel->isConstrained());
}

int UnifiedAsyncSearchListModel::relevantCount() const
{
    int count = m_filterModel->rowCount();
    if (m_busyIndicatorModel->active())
        ++count;

    return count;
}

bool UnifiedAsyncSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return m_sourceModel && m_sourceModel->canFetchMore();
}

void UnifiedAsyncSearchListModel::fetchMore(const QModelIndex& /*parent*/)
{
    m_sourceModel->fetchMore();
    m_busyIndicatorModel->setActive(m_sourceModel->fetchInProgress());
}

} // namespace desktop
} // namespace client
} // namespace nx
