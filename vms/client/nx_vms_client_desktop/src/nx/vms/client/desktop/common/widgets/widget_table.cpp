#include "widget_table.h"
#include "private/widget_table_p.h"

namespace nx::vms::client::desktop {

WidgetTable::WidgetTable(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
}

WidgetTable::~WidgetTable()
{
}

QAbstractItemModel* WidgetTable::model() const
{
    return d->model();
}

void WidgetTable::setModel(QAbstractItemModel* model, const QModelIndex& rootIndex)
{
    d->setModel(model, rootIndex);
}

QModelIndex WidgetTable::rootIndex() const
{
    return d->rootIndex();
}

void WidgetTable::setRootIndex(const QModelIndex& rootIndex)
{
    d->setRootIndex(rootIndex, false);
}

QHeaderView* WidgetTable::header() const
{
    return d->header();
}

void WidgetTable::setHeader(QHeaderView* header)
{
    d->setHeader(header);
}

WidgetTableDelegate* WidgetTable::itemDelegate() const
{
    return d->commonDelegate();
}

void WidgetTable::setItemDelegate(WidgetTableDelegate* newDelegate)
{
    d->setUserDelegate(newDelegate);
}

WidgetTableDelegate* WidgetTable::itemDelegateForColumn(int column) const
{
    return d->columnDelegate(column);
}

void WidgetTable::setItemDelegateForColumn(int column, WidgetTableDelegate* newDelegate)
{
    d->setColumnDelegate(column, newDelegate);
}

int WidgetTable::minimumRowHeight() const
{
    return d->minimumRowHeight();
}

void WidgetTable::setMinimumRowHeight(int height)
{
    d->setMinimumRowHeight(qMax(height, 0));
}

int WidgetTable::headerPadding() const
{
    return d->headerPadding();
}

void WidgetTable::setHeaderPadding(int padding)
{
    d->setHeaderPadding(qMax(padding, 0));
}

bool WidgetTable::headerVisible() const
{
    return d->headerVisible();
}

void WidgetTable::setHeaderVisible(bool visible)
{
    return d->setHeaderVisible(visible);
}

bool WidgetTable::sortingEnabled() const
{
    return d->sortingEnabled();
}

void WidgetTable::setSortingEnabled(bool enable)
{
    d->setSortingEnabled(enable);
}

int WidgetTable::rowSpacing() const
{
    return d->rowSpacing();
}

void WidgetTable::setRowSpacing(int spacing)
{
    d->setRowSpacing(qMax(spacing, 0));
}

QModelIndex WidgetTable::indexForWidget(QWidget* widget)
{
    return Private::indexForWidget(widget);
}

QWidget* WidgetTable::widgetForCell(int row, int column) const
{
    if (row < 0 || row >= d->rowCount())
        return nullptr;

    if (column < 0 || column >= d->columnCount())
        return nullptr;

    return d->widgetFor(row, column);
}

} // namespace nx::vms::client::desktop
