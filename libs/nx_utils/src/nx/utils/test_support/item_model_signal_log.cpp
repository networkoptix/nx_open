// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_model_signal_log.h"

#include <QtCore/QAbstractListModel>
#include <QtCore/QAbstractTableModel>

#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>

namespace nx::utils::test {

struct ItemModelSignalLog::Private
{
    const QPointer<QAbstractItemModel> model;
    const Options options;
    QStringList log;
    ScopedConnections connections;

    QString optionalParentStr(const QModelIndex& parent) const
    {
        return optionalParentStr(parent, options.testFlag(OmitParents) && !parent.isValid());
    }

    QString optionalParentStr(const QModelIndex& parent, bool omit) const
    {
        return omit
            ? QString{}
            : indexPath(parent) + ",";
    }

    QString optionalParentListStr(const QList<QPersistentModelIndex>& parents)
    {
        if (options.testFlag(OmitParents) && parents.empty())
            return {};

        QStringList result;
        for (const auto& parent: parents)
            result.append(indexPath(parent));

        return nx::format("[%1],", result.join(","));
    }

    QString optionalRowCountStr(const QModelIndex& parent) const
    {
        if (!options.testFlag(AppendCounts))
            return {};

        return nx::format(" |%1", model->rowCount(parent));
    }

    QString optionalRowCountStr(
        const QModelIndex& sourceParent, const QModelIndex& destinationParent) const
    {
        if (!options.testFlag(AppendCounts))
            return {};

        return sourceParent == destinationParent
            ? nx::format(" |%1", model->rowCount(sourceParent))
            : nx::format(" |%1 |%2", model->rowCount(sourceParent),
                model->rowCount(destinationParent));
    }

    QString optionalColumnCountStr(const QModelIndex& parent) const
    {
        if (!options.testFlag(AppendCounts))
            return {};

        return nx::format(" |%1", model->columnCount(parent));
    }

    QString optionalColumnCountStr(
        const QModelIndex& sourceParent, const QModelIndex& destinationParent) const
    {
        if (!options.testFlag(AppendCounts))
            return {};

        return sourceParent == destinationParent
            ? nx::format(" |%1", model->columnCount(sourceParent))
            : nx::format(" |%1 |%2", model->columnCount(sourceParent),
                model->columnCount(destinationParent));
    }

    QString roleListStr(const QList<int>& roles) const
    {
        QStringList result;
        for (const int role: roles)
            result.append(QString::number(role));

        return nx::format("[%1]", result.join(","));
    }

    QString indexStr(const QModelIndex& index) const
    {
        if (!index.isValid())
            return "empty";

        return options.testFlag(OmitColumns) && index.column() == 0
            ? nx::format("%1", index.row())
            : nx::format("%1:%2", index.row(), index.column());
    }

    QString indexPath(const QModelIndex& index) const
    {
        return index.parent().isValid()
            ? (indexPath(index.parent()) + "/" + indexStr(index))
            : indexStr(index);
    }

    static Options defaultOptions(QAbstractItemModel* model)
    {
        ItemModelSignalLog::Options options = ItemModelSignalLog::AppendCounts;

        const bool isList = (bool) qobject_cast<QAbstractListModel*>(model);
        const bool isTable = (bool) qobject_cast<QAbstractTableModel*>(model);

        if (isTable || isList)
            options |= ItemModelSignalLog::OmitParents;

        if (!isTable)
            options |= ItemModelSignalLog::OmitColumns;

        return options;
    }
};

ItemModelSignalLog::ItemModelSignalLog(QAbstractItemModel* model, QObject* parent):
    ItemModelSignalLog(model, Private::defaultOptions(model), parent)
{
}

ItemModelSignalLog::ItemModelSignalLog(QAbstractItemModel* model, Options options, QObject* parent):
    QObject(parent),
    d(new Private{.model = model, .options = options})
{
    if (!NX_ASSERT(d->model))
        return;

    d->connections << connect(d->model.get(), &QAbstractItemModel::modelAboutToBeReset, this,
        [this]() { d->log.append("modelAboutToBeReset()"); }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::modelReset, this,
        [this]() { d->log.append("modelReset()"); }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::rowsAboutToBeInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->log.append(nx::format("rowsAboutToBeInserted(%1%2,%3)%4",
                d->optionalParentStr(parent), first, last, d->optionalRowCountStr(parent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::rowsInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->log.append(nx::format("rowsInserted(%1%2,%3)%4",
                d->optionalParentStr(parent), first, last, d->optionalRowCountStr(parent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->log.append(nx::format("rowsAboutToBeRemoved(%1%2,%3)%4",
                d->optionalParentStr(parent), first, last, d->optionalRowCountStr(parent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::rowsRemoved, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->log.append(nx::format("rowsRemoved(%1%2,%3)%4",
                d->optionalParentStr(parent), first, last, d->optionalRowCountStr(parent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::rowsAboutToBeMoved, this,
        [this](const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
            const QModelIndex& destinationParent, int destinationRow)
        {
            const bool omitParents = d->options.testFlag(OmitParents)
                && !destinationParent.isValid()
                && !sourceParent.isValid();

            d->log.append(nx::format("rowsAboutToBeMoved(%1%2,%3,%4%5)%6",
                d->optionalParentStr(sourceParent, omitParents), sourceFirst, sourceLast,
                d->optionalParentStr(destinationParent, omitParents), destinationRow,
                d->optionalRowCountStr(sourceParent, destinationParent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::rowsMoved, this,
        [this](const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
            const QModelIndex& destinationParent, int destinationRow)
        {
            const bool omitParents = d->options.testFlag(OmitParents)
                && !destinationParent.isValid()
                && !sourceParent.isValid();

            d->log.append(nx::format("rowsMoved(%1%2,%3,%4%5)%6",
                d->optionalParentStr(sourceParent, omitParents), sourceFirst, sourceLast,
                d->optionalParentStr(destinationParent, omitParents), destinationRow,
                d->optionalRowCountStr(sourceParent, destinationParent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::columnsAboutToBeInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->log.append(nx::format("columnsAboutToBeInserted(%1%2,%3)%4",
                d->optionalParentStr(parent), first, last, d->optionalColumnCountStr(parent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::columnsInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->log.append(nx::format("columnsInserted(%1%2,%3)%4",
                d->optionalParentStr(parent), first, last, d->optionalColumnCountStr(parent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::columnsAboutToBeRemoved, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->log.append(nx::format("columnsAboutToBeRemoved(%1%2,%3)%4",
                d->optionalParentStr(parent), first, last, d->optionalColumnCountStr(parent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::columnsRemoved, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            d->log.append(nx::format("columnsRemoved(%1%2,%3)%4",
                d->optionalParentStr(parent), first, last, d->optionalColumnCountStr(parent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::columnsAboutToBeMoved, this,
        [this](const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
            const QModelIndex& destinationParent, int destinationColumn)
        {
            const bool omitParents = d->options.testFlag(OmitParents)
                && !destinationParent.isValid()
                && !sourceParent.isValid();

            d->log.append(nx::format("columnsAboutToBeMoved(%1%2,%3,%4%5)%6",
                d->optionalParentStr(sourceParent, omitParents), sourceFirst, sourceLast,
                d->optionalParentStr(destinationParent, omitParents), destinationColumn,
                d->optionalColumnCountStr(sourceParent, destinationParent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::columnsMoved, this,
        [this](const QModelIndex& sourceParent, int sourceFirst, int sourceLast,
            const QModelIndex& destinationParent, int destinationColumn)
        {
            const bool omitParents = d->options.testFlag(OmitParents)
                && !destinationParent.isValid()
                && !sourceParent.isValid();

            d->log.append(nx::format("columnsMoved(%1%2,%3,%4%5)%6",
                d->optionalParentStr(sourceParent, omitParents), sourceFirst, sourceLast,
                d->optionalParentStr(destinationParent, omitParents), destinationColumn,
                d->optionalColumnCountStr(sourceParent, destinationParent)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::layoutAboutToBeChanged, this,
        [this](const QList<QPersistentModelIndex>& parents,
            QAbstractItemModel::LayoutChangeHint hint)
        {
            d->log.append(nx::format("layoutAboutToBeChanged(%1%2)",
                d->optionalParentListStr(parents), hint));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::layoutChanged, this,
        [this](const QList<QPersistentModelIndex>& parents,
            QAbstractItemModel::LayoutChangeHint hint)
        {
            d->log.append(nx::format("layoutChanged(%1%2)",
                d->optionalParentListStr(parents), hint));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
        {
            d->log.append(nx::format("dataChanged(%1,%2,%3)",
                d->indexPath(topLeft), d->indexPath(bottomRight), d->roleListStr(roles)));
        }, Qt::DirectConnection);

    d->connections << connect(d->model.get(), &QAbstractItemModel::headerDataChanged, this,
        [this](Qt::Orientation orientation, int first, int last)
        {
            d->log.append(nx::format("headerDataChanged(%1,%2,%3)", orientation, first, last));
        }, Qt::DirectConnection);
}

ItemModelSignalLog::~ItemModelSignalLog()
{
    // Required here for forward-declared scoped pointer destruction.
}

QStringList ItemModelSignalLog::log() const
{
    return d->log;
}

void ItemModelSignalLog::clear()
{
    d->log.clear();
}

void ItemModelSignalLog::addCustomEntry(const QString& entry)
{
    d->log.append(entry);
}

QAbstractItemModel* ItemModelSignalLog::model() const
{
    return d->model;
}

ItemModelSignalLog::Options ItemModelSignalLog::options() const
{
    return d->options;
}

} // namespace nx::utils::test
