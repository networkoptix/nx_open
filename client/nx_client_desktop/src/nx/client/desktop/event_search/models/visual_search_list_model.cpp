#include "visual_search_list_model.h"

#include <utils/common/delayed.h>

#include <nx/client/desktop/event_search/models/abstract_search_list_model.h>
#include <nx/client/desktop/event_search/models/private/busy_indicator_model_p.h>

namespace nx::client::desktop {

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

    m_headIndicatorModel->setVisible(false);
    m_tailIndicatorModel->setVisible(true);

    connect(m_sourceModel, &AbstractSearchListModel::fetchFinished,
        this, &VisualSearchListModel::handleFetchFinished);
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
    if (value == fetchDirection())
        return;

    auto prevIndicator = relevantIndicatorModel();
    prevIndicator->setActive(false);
    prevIndicator->setVisible(true);

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

BusyIndicatorModel* VisualSearchListModel::relevantIndicatorModel() const
{
    return (fetchDirection() == FetchDirection::earlier)
        ? m_tailIndicatorModel
        : m_headIndicatorModel;
}

void VisualSearchListModel::fetchMore(const QModelIndex& /*parent*/)
{
    m_sourceModel->fetchMore();

    auto indicator = relevantIndicatorModel();
    const bool active = m_sourceModel->fetchInProgress();
    indicator->setActive(active);
    if (active)
        indicator->setVisible(true); //< All hiding is done in handleFetchFinished.
}

void VisualSearchListModel::handleFetchFinished()
{
    relevantIndicatorModel()->setVisible(canFetchMore());
}

} // namespace nx::client::desktop
