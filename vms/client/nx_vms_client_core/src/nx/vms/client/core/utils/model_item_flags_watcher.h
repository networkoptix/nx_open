// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>
#include <QtCore/QObject>

namespace nx::vms::client::core {

/**
 * This helper emits a notification signal when a watched model index is within a dataChanged
 * range and its itemFlags are probably changed.
 */
class ModelItemFlagsWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QModelIndex index READ index WRITE setIndex NOTIFY indexChanged)
    Q_PROPERTY(Qt::ItemFlags itemFlags READ itemFlags NOTIFY itemFlagsChanged)

    using base_type = QObject;

public:
    explicit ModelItemFlagsWatcher(QObject* parent = nullptr): QObject(parent) {}

    QModelIndex index() const;
    void setIndex(const QModelIndex& value);

    Qt::ItemFlags itemFlags() const { return m_itemFlags; }

    static void registerQmlType();

private:
    void setItemFlags(Qt::ItemFlags itemFlags);

signals:
    void indexChanged();
    void itemFlagsChanged();

private:
    QPersistentModelIndex m_index;
    Qt::ItemFlags m_itemFlags;
};

} // namespace nx::vms::client::core
