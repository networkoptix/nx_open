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
    // If an invalid index is set over an invalidated persistent index, process it as a change.
    // Invalidated persistent index can be detected by m_model != nullptr.
    if (m_index == value && (m_index.isValid() || !m_model))
        return;

    const bool modelChanged = m_model != value.model();
    if (modelChanged && m_model)
        m_model->disconnect(this);

    m_index = value;
    m_model = m_index.model();
    m_itemFlags = value.flags();

    if (modelChanged && m_model)
    {
        connect(m_model.data(), &QAbstractItemModel::dataChanged, this,
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
