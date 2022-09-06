// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "model_item_flags_watcher.h"

#include <QtCore/QAbstractItemModel>
#include <QtQml/QtQml>

namespace nx::vms::client::core {

QModelIndex ModelItemFlagsWatcher::index() const
{
    if (m_index.isValid())
        return m_index;

    return {};
}

void ModelItemFlagsWatcher::setIndex(const QModelIndex& value)
{
    if (m_index == value)
        return;

    const bool modelChanged = m_index.model() != value.model();

    if (modelChanged && m_index.isValid())
        m_index.model()->disconnect(this);

    m_index = value;
    m_itemFlags = value.flags();

    if (modelChanged && m_index.isValid())
    {
        connect(m_index.model(), &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& topLeft, const QModelIndex& bottomRight)
            {
                if (m_index.isValid()
                    && m_index.parent() == topLeft.parent()
                    && m_index.row() >= topLeft.row()
                    && m_index.row() <= bottomRight.row()
                    && m_index.column() >= topLeft.column()
                    && m_index.column() <= bottomRight.column())
                {
                    setItemFlags(m_index.flags());
                }
            });
    }

    emit indexChanged();
    emit itemFlagsChanged();
}

void ModelItemFlagsWatcher::registerQmlType()
{
    qmlRegisterType<ModelItemFlagsWatcher>("nx.vms.client.core", 1, 0, "ModelItemFlagsWatcher");
}

void ModelItemFlagsWatcher::setItemFlags(Qt::ItemFlags itemFlags)
{
    if (m_itemFlags == itemFlags)
        return;

    m_itemFlags = itemFlags;
    emit itemFlagsChanged();
}

} // namespace nx::vms::client::core
