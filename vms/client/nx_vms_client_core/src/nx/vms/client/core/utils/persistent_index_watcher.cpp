// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "persistent_index_watcher.h"

#include <QtQml/QtQml>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::core {

// ------------------------------------------------------------------------------------------------
// PersistentIndexWatcher::Private

struct PersistentIndexWatcher::Private
{
    PersistentIndexWatcher* const q;
    QPersistentModelIndex index;
    nx::utils::ScopedConnections modelConnections;

    // Saved state.
    int row = -1;
    int column = -1;
    QPersistentModelIndex parent;

    void checkUpdate()
    {
        if (index.parent() == parent && index.row() == row && index.column() == column)
            return;

        parent = index.parent();
        row = index.row();
        column = index.column();

        if (!index.model())
            modelConnections.reset();

        emit q->indexChanged();
    }

    void setIndex(const QModelIndex& value)
    {
        if (index == value)
            return;

        index = value;

        parent = index.parent();
        row = index.row();
        column = index.column();

        modelConnections.reset();
        if (index.model())
        {
            modelConnections
                << QObject::connect(index.model(), &QAbstractItemModel::modelReset, q,
                    [this]() { checkUpdate(); });

            modelConnections
                << QObject::connect(index.model(), &QAbstractItemModel::layoutChanged, q,
                    [this]() { checkUpdate(); });

            modelConnections
                << QObject::connect(index.model(), &QAbstractItemModel::rowsInserted, q,
                    [this]() { checkUpdate(); });

            modelConnections
                << QObject::connect(index.model(), &QAbstractItemModel::rowsRemoved, q,
                    [this]() { checkUpdate(); });

            modelConnections
                << QObject::connect(index.model(), &QAbstractItemModel::rowsMoved, q,
                    [this]() { checkUpdate(); });

            modelConnections
                << QObject::connect(index.model(), &QAbstractItemModel::columnsInserted, q,
                    [this]() { checkUpdate(); });

            modelConnections
                << QObject::connect(index.model(), &QAbstractItemModel::columnsRemoved, q,
                    [this]() { checkUpdate(); });

            modelConnections
                << QObject::connect(index.model(), &QAbstractItemModel::columnsMoved, q,
                    [this]() { checkUpdate(); });
        }

        emit q->indexChanged();
    }
};

// ------------------------------------------------------------------------------------------------
// PersistentIndexWatcher

PersistentIndexWatcher::PersistentIndexWatcher(QObject* parent):
    QObject(parent),
    d(new Private{this})
{
}

PersistentIndexWatcher::~PersistentIndexWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

QModelIndex PersistentIndexWatcher::index() const
{
    return d->index;
}

void PersistentIndexWatcher::setIndex(const QModelIndex& value)
{
    d->setIndex(value);
}

void PersistentIndexWatcher::registerQmlType()
{
    qmlRegisterType<PersistentIndexWatcher>("nx.vms.client.core", 1, 0, "PersistentIndexWatcher");
}

} // namespace nx::vms::client::core
