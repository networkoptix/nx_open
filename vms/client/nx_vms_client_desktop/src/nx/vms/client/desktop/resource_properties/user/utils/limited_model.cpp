// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "limited_model.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {

LimitedModel::LimitedModel()
{
    connect(this, &QSortFilterProxyModel::sourceModelChanged,
        [this]()
        {
            if (!sourceModel())
            {
                m_modelConnections.reset();
                sourceRowsChanged();
                return;
            }

            nx::utils::ScopedConnections modelConnections;

            modelConnections << connect(sourceModel(), &QSortFilterProxyModel::rowsInserted,
                this, &LimitedModel::sourceRowsChanged, Qt::QueuedConnection);
            modelConnections << connect(sourceModel(), &QSortFilterProxyModel::rowsRemoved,
                this, &LimitedModel::sourceRowsChanged, Qt::QueuedConnection);
            modelConnections << connect(sourceModel(), &QSortFilterProxyModel::modelReset,
                this, &LimitedModel::sourceRowsChanged, Qt::QueuedConnection);

            m_modelConnections = std::move(modelConnections);
        });
}

void LimitedModel::sourceRowsChanged()
{
    sourceCountChanged();
    updateRemaining();
}

void LimitedModel::updateRemaining()
{
    const int remaining = sourceModel()
        ? std::max(sourceModel()->rowCount() - m_limit, 0)
        : 0;

    if (remaining == m_remaining)
        return;

    m_remaining = remaining;
    emit remainingChanged();
}

bool LimitedModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_limit > 0 && sourceRow >= m_limit)
        return false;
    return base_type::filterAcceptsRow(sourceRow, sourceParent);
}

void LimitedModel::setLimit(int limit)
{
    if (m_limit == limit)
        return;

    m_limit = limit;
    emit limitChanged();

    updateRemaining();

    invalidateRowsFilter();
}

void LimitedModel::registerQmlType()
{
    qmlRegisterType<LimitedModel>("nx.vms.client.desktop", 1, 0, "LimitedModel");
}

} // namespace nx::vms::client::desktop
