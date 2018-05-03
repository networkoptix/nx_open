#include "column_remap_proxy_model.h"

#include <algorithm>

namespace nx {
namespace client {
namespace desktop {

ColumnRemapProxyModel::ColumnRemapProxyModel(
    const QVector<int>& sourceColumns,
    QObject* parent)
    :
    base_type(parent),
    m_proxyToSource(sourceColumns)
{
}

ColumnRemapProxyModel::~ColumnRemapProxyModel()
{
}

int ColumnRemapProxyModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return m_proxyToSource.size();
}

int ColumnRemapProxyModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !sourceModel())
        return 0;

    return sourceModel()->rowCount();
}

QModelIndex ColumnRemapProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid()
        || row < 0 || row >= rowCount()
        || column < 0 || column >= columnCount())
    {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex ColumnRemapProxyModel::sibling(int row, int column, const QModelIndex& index) const
{
    return this->index(row, column, index.parent());
}

QModelIndex ColumnRemapProxyModel::parent(const QModelIndex& /*index*/) const
{
    return QModelIndex();
}

QModelIndex ColumnRemapProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid()
        || !sourceModel()
        || proxyIndex.model() != this
        || proxyIndex.column() >= m_proxyToSource.size())
    {
        return QModelIndex();
    }

    const auto sourceColumn = m_proxyToSource[proxyIndex.column()];
    return sourceModel()->index(proxyIndex.row(), sourceColumn);
}

QModelIndex ColumnRemapProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid()
        || !sourceModel()
        || sourceIndex.model() != sourceModel()
        || sourceIndex.column() >= m_sourceToProxy.size())
    {
        return QModelIndex();
    }

    const auto proxyColumn = m_sourceToProxy[sourceIndex.column()];
    return createIndex(sourceIndex.row(), proxyColumn);
}

void ColumnRemapProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    base_type::setSourceModel(sourceModel);
    m_sourceToProxy.clear();

    if (!sourceModel)
        return;

    m_sourceToProxy.insert(0, sourceModel->columnCount(), -1);

    for (int i = 0; i < m_proxyToSource.size(); ++i)
    {
        const int sourceColumn = m_proxyToSource[i];

        if (sourceColumn >= 0 && sourceColumn < m_sourceToProxy.size())
            m_sourceToProxy[sourceColumn] = i;
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
