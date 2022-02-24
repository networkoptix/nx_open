// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "list_navigation_helper.h"

#include <algorithm>

#include <nx/utils/log/assert.h>

#include <QtCore/QItemSelectionModel>
#include <QtCore/QScopedValueRollback>
#include <QtQml/QtQml>

namespace nx::vms::client::desktop {

namespace {

bool canNavigateTo(const QModelIndex& index)
{
    static const Qt::ItemFlags kRequiredFlags{Qt::ItemIsSelectable | Qt::ItemIsEnabled};
    return NX_ASSERT(index.isValid()) && (index.flags() & kRequiredFlags) == kRequiredFlags;
};

int lastRow(const QModelIndex& index)
{
    return NX_ASSERT(index.isValid())
        ? index.model()->rowCount(index.parent()) - 1
        : -1;
}

QModelIndex findBackward(const QModelIndex& current, int rowFrom)
{
    for (int row = rowFrom; row >= 0; --row)
    {
        const auto index = current.siblingAtRow(row);
        if (canNavigateTo(index))
            return index;
    }

    return current;
};

QModelIndex findForward(const QModelIndex& current, int rowFrom)
{
    for (int row = rowFrom, last = lastRow(current); row <= last; ++row)
    {
        const auto index = current.siblingAtRow(row);
        if (canNavigateTo(index))
            return index;
    }

    return current;
};

QItemSelectionModel::SelectionFlags selectionFlags(Qt::KeyboardModifiers modifiers)
{
    if (modifiers.testFlag(Qt::ShiftModifier))
        return QItemSelectionModel::SelectionFlag::SelectCurrent;

    return modifiers.testFlag(Qt::ControlModifier)
        ? QItemSelectionModel::SelectionFlag::Toggle
        : QItemSelectionModel::SelectionFlag::ClearAndSelect;
}

} // namespace

struct ListNavigationHelper::Private
{
    QPointer<QItemSelectionModel> selectionModel;
    QPersistentModelIndex savedCurrentIndex;
    bool preserveSavedCurrentIndex = false;
    int pageSize = 10; //< Default items per page.
};

ListNavigationHelper::ListNavigationHelper(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

ListNavigationHelper::~ListNavigationHelper()
{
    // Required here for forward-declared scoped pointer destruction.
}

QItemSelectionModel* ListNavigationHelper::selectionModel() const
{
    return d->selectionModel;
}

void ListNavigationHelper::setSelectionModel(QItemSelectionModel* value)
{
    if (d->selectionModel == value)
        return;

    if (d->selectionModel)
        d->selectionModel->disconnect(this);

    d->selectionModel = value;
    d->savedCurrentIndex = QPersistentModelIndex();

    if (d->selectionModel)
    {
        d->savedCurrentIndex = d->selectionModel->currentIndex();

        connect(d->selectionModel.data(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current)
            {
                if (!d->preserveSavedCurrentIndex)
                    d->savedCurrentIndex = current;
            });
    }

    emit selectionModelChanged();
}

int ListNavigationHelper::pageSize() const
{
    return d->pageSize;
}

void ListNavigationHelper::setPageSize(int value)
{
    value = std::max(value, 1); //< For safety.

    if (d->pageSize == value)
        return;

    d->pageSize = value;
    emit pageSizeChanged();
}

void ListNavigationHelper::navigate(Move type, Qt::KeyboardModifiers modifiers)
{
    if (!NX_ASSERT(d->selectionModel))
        return;

    const auto currentIndex = d->selectionModel->currentIndex();
    if (!currentIndex.isValid())
        return;

    QModelIndex target;
    switch (type)
    {
        case Move::up:
            target = findBackward(currentIndex, currentIndex.row() - 1);
            break;

        case Move::down:
            target = findForward(currentIndex, currentIndex.row() + 1);
            break;

        case Move::home:
            target = findForward(currentIndex, 0);
            break;

        case Move::end:
            target = findBackward(currentIndex, lastRow(currentIndex));
            break;

        case Move::pageUp:
            target = currentIndex.siblingAtRow(std::max(currentIndex.row() - d->pageSize, 0));
            target = findBackward(target, target.row());
            target = findForward(target, target.row());
            break;

        case Move::pageDown:
            target = currentIndex.siblingAtRow(std::min(currentIndex.row() + d->pageSize,
                lastRow(currentIndex)));
            target = findForward(target, target.row());
            target = findBackward(target, target.row());
            break;
    }

    if (!canNavigateTo(target))
        return;

    if (modifiers.testFlag(Qt::ControlModifier) && !modifiers.testFlag(Qt::ShiftModifier))
    {
        d->selectionModel->setCurrentIndex(target, QItemSelectionModel::SelectionFlag::NoUpdate);
        d->selectionModel->select(QItemSelection(), QItemSelectionModel::SelectionFlag::Select);
    }
    else
    {
        navigateTo(target, modifiers);
    }
}

void ListNavigationHelper::navigateTo(const QModelIndex& index, Qt::KeyboardModifiers modifiers)
{
    if (!NX_ASSERT(d->selectionModel) || !canNavigateTo(index))
        return;

    {
        const QScopedValueRollback<bool> savedCurrentIndexGuard(d->preserveSavedCurrentIndex,
            modifiers.testFlag(Qt::ShiftModifier));

        d->selectionModel->setCurrentIndex(index, selectionFlags(modifiers));
    }

    if (!d->savedCurrentIndex.isValid()
        || !NX_ASSERT(d->selectionModel->currentIndex().parent() == d->savedCurrentIndex.parent()))
    {
        d->savedCurrentIndex = d->selectionModel->currentIndex();
    }

    if (modifiers.testFlag(Qt::ShiftModifier))
    {
        d->selectionModel->select(
            QItemSelection(d->savedCurrentIndex, d->selectionModel->currentIndex()),
            QItemSelectionModel::SelectCurrent);
    }
}

void ListNavigationHelper::registerQmlType()
{
    qmlRegisterType<ListNavigationHelper>("nx.vms.client.desktop", 1, 0, "ListNavigationHelper");
}

} // nx::vms::client::desktop
