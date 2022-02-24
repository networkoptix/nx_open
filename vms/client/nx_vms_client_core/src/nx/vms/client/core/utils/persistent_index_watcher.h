// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

/**
 * This helper emits `indexChanged` signal if either a new index was set, or a previously set
 * index (stored as a persistent index) was changed internally. An index is considered changed if
 * its row, column or parent (also stored as a persistent index) is changed.
 */
class PersistentIndexWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QModelIndex index READ index WRITE setIndex NOTIFY indexChanged)

public:
    PersistentIndexWatcher(QObject* parent = nullptr);
    virtual ~PersistentIndexWatcher() override;

    QModelIndex index() const;
    void setIndex(const QModelIndex& value);

    static void registerQmlType();

signals:
    void indexChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
