// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "row_count_watcher.h"

#include <QtCore/QPointer>
#include <QtQml/QtQml>

namespace nx::vms::client::core {

class RowCountWatcher::Private: public QObject
{
    RowCountWatcher* const q;

public:
    Private(RowCountWatcher* q): q(q)
    {
    }

    bool isValid() const
    {
        // There is no assertion for parentIndex.model() == model to allow setting these two
        // properties from QML in any order.
        return !model.isNull()
            && (!parentIndex.isValid() || parentIndex.model() == model)
            && (topLevel || parentIndex.isValid());
    }

    int calculateRowCount() const
    {
        return isValid()
            ? model->rowCount(parentIndex)
            : 0;
    }

    int calculateRecursiveRowCount() const
    {
        return recursiveTracking && isValid()
            ? calculateRecursiveChildCount(parentIndex)
            : 0;
    }

    int calculateRecursiveChildCount(const QModelIndex& parent) const
    {
        if (parent.flags().testFlag(Qt::ItemNeverHasChildren))
            return 0;

        const int count = model->rowCount(parent);
        int result = count;

        for (int row = 0; row < count; ++row)
            result += calculateRecursiveChildCount(model->index(row, 0, parent));

        return result;
    }

    void setRowCount(int value)
    {
        if (rowCount == value)
            return;

        rowCount = value;
        emit q->rowCountChanged();
    }

    void setRecursiveRowCount(int value)
    {
        if (recursiveRowCount == value)
            return;

        recursiveRowCount = value;
        emit q->recursiveRowCountChanged();
    }

    void updateRowCounts()
    {
        setRowCount(calculateRowCount());
        setRecursiveRowCount(calculateRecursiveRowCount());
    }

public:
    QPointer<QAbstractItemModel> model;
    QPersistentModelIndex parentIndex;
    bool recursiveTracking = false;
    bool topLevel = true; //< `parentIndex` might get cleared due to row deletion.
    int rowCount = 0;
    int recursiveRowCount = 0;
};

RowCountWatcher::RowCountWatcher(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

RowCountWatcher::~RowCountWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

QAbstractItemModel* RowCountWatcher::model() const
{
    return d->model;
}

void RowCountWatcher::setModel(QAbstractItemModel* value)
{
    if (d->model == value)
        return;

    if (d->model)
        d->model->disconnect(d.get());

    d->model = value;

    if (d->model)
    {
        connect(d->model, &QAbstractItemModel::modelReset, d.get(), &Private::updateRowCounts);
        connect(d->model, &QAbstractItemModel::rowsInserted, d.get(), &Private::updateRowCounts);
        connect(d->model, &QAbstractItemModel::rowsRemoved, d.get(), &Private::updateRowCounts);
        connect(d->model, &QAbstractItemModel::rowsMoved, d.get(), &Private::updateRowCounts);
        connect(d->model, &QAbstractItemModel::layoutChanged, d.get(), &Private::updateRowCounts);
    }

    emit modelChanged();
    d->updateRowCounts();
}

QModelIndex RowCountWatcher::parentIndex() const
{
    return d->parentIndex;
}

void RowCountWatcher::setParentIndex(const QModelIndex& value)
{
    if (d->parentIndex == value)
        return;

    d->parentIndex = value;
    d->topLevel = !value.isValid();

    emit parentIndexChanged();

    d->updateRowCounts();
}

bool RowCountWatcher::recursiveTracking() const
{
    return d->recursiveTracking;
}

void RowCountWatcher::setRecursiveTracking(bool value)
{
    if (d->recursiveTracking == value)
        return;

    d->recursiveTracking = value;
    emit recursiveTrackingChanged();

    d->updateRowCounts();
}

int RowCountWatcher::rowCount() const
{
    return d->rowCount;
}

int RowCountWatcher::recursiveRowCount() const
{
    return d->recursiveRowCount;
}

void RowCountWatcher::registerQmlType()
{
    qmlRegisterType<RowCountWatcher>("nx.vms.client.core", 1, 0, "RowCountWatcher");
}

} // namespace nx::vms::client::core
