#include "visual_search_list_model.h"

#include <utils/common/delayed.h>

#include <nx/client/desktop/event_search/models/abstract_search_list_model.h>
#include <nx/client/desktop/event_search/models/private/busy_indicator_model_p.h>

namespace nx {
namespace client {
namespace desktop {

VisualSearchListModel::VisualSearchListModel(
    AbstractSearchListModel* sourceModel,
    QObject* parent)
    :
    base_type(parent),
    m_sourceModel(sourceModel),
    m_headIndicatorModel(new BusyIndicatorModel(this)),
    m_tailIndicatorModel(new BusyIndicatorModel(this))
{
    NX_ASSERT(sourceModel);
    setModels({
        m_headIndicatorModel,
        m_sourceModel,
        m_tailIndicatorModel});

    connect(m_sourceModel, &AbstractSearchListModel::fetchFinished, this,
        [this]()
        {
            m_headIndicatorModel->setActive(false);
            m_tailIndicatorModel->setActive(false);
        });
}

VisualSearchListModel::~VisualSearchListModel()
{
}

QnTimePeriod VisualSearchListModel::relevantTimePeriod() const
{
    return m_sourceModel->relevantTimePeriod();
}

void VisualSearchListModel::setRelevantTimePeriod(const QnTimePeriod& value)
{
    m_sourceModel->setRelevantTimePeriod(value);
}

VisualSearchListModel::FetchDirection VisualSearchListModel::fetchDirection() const
{
    return m_sourceModel->fetchDirection();
}

void VisualSearchListModel::setFetchDirection(FetchDirection value)
{
    m_sourceModel->setFetchDirection(value);
}

bool VisualSearchListModel::isConstrained() const
{
    return m_sourceModel && m_sourceModel->isConstrained();
}

int VisualSearchListModel::relevantCount() const
{
    return m_sourceModel ? m_sourceModel->rowCount() : 0;
}

bool VisualSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return m_sourceModel && m_sourceModel->canFetchMore();
}

void VisualSearchListModel::fetchMore(const QModelIndex& /*parent*/)
{
    m_sourceModel->fetchMore();

    auto indicatorModel = fetchDirection() == FetchDirection::earlier
        ? m_tailIndicatorModel
        : m_headIndicatorModel;

    indicatorModel->setActive(m_sourceModel->fetchInProgress());
}

} // namespace desktop
} // namespace client
} // namespace nx
