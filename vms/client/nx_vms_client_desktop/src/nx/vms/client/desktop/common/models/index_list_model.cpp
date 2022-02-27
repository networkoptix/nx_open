// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "index_list_model.h"

#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {

int IndexListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_source.size();
}

QVariant IndexListModel::data(const QModelIndex& index, int role) const
{
    return index.isValid() && NX_ASSERT(checkIndex(index))
        ? m_source[index.row()].data(role)
        : QVariant();
}

Qt::ItemFlags IndexListModel::flags(const QModelIndex& index) const
{
    return index.isValid() && NX_ASSERT(checkIndex(index))
        ? m_source[index.row()].flags()
        : Qt::ItemFlags();
}

QHash<int, QByteArray> IndexListModel::roleNames() const
{
    return m_roleNames;
}

QModelIndex IndexListModel::sourceIndex(int row) const
{
    if (row >= 0 && row < m_source.size())
        return m_source[row];
    return {};
}

QModelIndexList IndexListModel::source() const
{
    QModelIndexList source;
    std::copy(m_source.cbegin(), m_source.cend(), std::back_inserter(source));
    return source;
}

void IndexListModel::setSource(const QModelIndexList& value)
{
    const QAbstractItemModel* model = nullptr;
    QList<QPersistentModelIndex> newSource;

    for (const auto& index: value)
    {
        if (!index.isValid() || !index.model())
            continue;

        newSource.push_back(index);
        if (model)
            NX_ASSERT(model == index.model());
        else
            model = index.model();
    }

    if (m_source == newSource)
        return;

    const auto notifier = nx::utils::makeScopeGuard([this]() { emit sourceChanged({}); });

    ScopedReset scopedReset(this);
    m_source = newSource;
    m_roleNames = model ? model->roleNames() : decltype(m_roleNames)();
}

void IndexListModel::registerQmlType()
{
    qmlRegisterType<IndexListModel>("nx.vms.client.desktop", 1, 0, "IndexListModel");
}

} // namespace nx::vms::client::desktop
