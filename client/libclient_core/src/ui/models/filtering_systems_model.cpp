
#include "filtering_systems_model.h"

#include <ui/models/ordered_systems_model.h>

QnFilteringSystemsModel::QnFilteringSystemsModel(QObject* parent) :
    base_type(parent),
    m_sourceRowsCount(0)
{
    setSourceModel(new QnOrderedSystemsModel(this));

    connect(sourceModel(), &QAbstractItemModel::modelReset,
        this, &QnFilteringSystemsModel::updateSourceRowsCount);
    connect(sourceModel(), &QAbstractItemModel::rowsRemoved,
        this, &QnFilteringSystemsModel::updateSourceRowsCount);
    connect(sourceModel(), &QAbstractItemModel::rowsInserted,
        this, &QnFilteringSystemsModel::updateSourceRowsCount);

    updateSourceRowsCount();
}

void QnFilteringSystemsModel::updateSourceRowsCount()
{
    const auto source = sourceModel();
    setSourceRowsCount(source->rowCount());
}

int QnFilteringSystemsModel::sourceRowsCount() const
{
    return m_sourceRowsCount;
}

void QnFilteringSystemsModel::setSourceRowsCount(int value)
{
    if (m_sourceRowsCount == value)
        return;

    m_sourceRowsCount = value;
    emit sourceRowsCountChanged();
}

