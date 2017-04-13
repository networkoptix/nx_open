#include "widget_table.h"
#include "widget_table_p.h"


QnWidgetTable::QnWidgetTable(QWidget* parent) :
    base_type(parent),
    d_ptr(new QnWidgetTablePrivate(this))
{
}

QnWidgetTable::~QnWidgetTable()
{
}

QAbstractItemModel* QnWidgetTable::model() const
{
    Q_D(const QnWidgetTable);
    return d->model();
}

void QnWidgetTable::setModel(QAbstractItemModel* model, const QModelIndex& rootIndex)
{
    Q_D(QnWidgetTable);
    d->setModel(model, rootIndex);
}

QModelIndex QnWidgetTable::rootIndex() const
{
    Q_D(const QnWidgetTable);
    return d->rootIndex();
}

void QnWidgetTable::setRootIndex(const QModelIndex& rootIndex)
{
    Q_D(QnWidgetTable);
    d->setRootIndex(rootIndex, false);
}

QHeaderView* QnWidgetTable::header() const
{
    Q_D(const QnWidgetTable);
    return d->header();
}

void QnWidgetTable::setHeader(QHeaderView* header)
{
    Q_D(QnWidgetTable);
    d->setHeader(header);
}

QnWidgetTableDelegate* QnWidgetTable::itemDelegate() const
{
    Q_D(const QnWidgetTable);
    return d->commonDelegate();
}

void QnWidgetTable::setItemDelegate(QnWidgetTableDelegate* newDelegate)
{
    Q_D(QnWidgetTable);
    d->setUserDelegate(newDelegate);
}

QnWidgetTableDelegate* QnWidgetTable::itemDelegateForColumn(int column) const
{
    Q_D(const QnWidgetTable);
    return d->columnDelegate(column);
}

void QnWidgetTable::setItemDelegateForColumn(int column, QnWidgetTableDelegate* newDelegate)
{
    Q_D(QnWidgetTable);
    d->setColumnDelegate(column, newDelegate);
}

int QnWidgetTable::minimumRowHeight() const
{
    Q_D(const QnWidgetTable);
    return d->minimumRowHeight();
}

void QnWidgetTable::setMinimumRowHeight(int height)
{
    Q_D(QnWidgetTable);
    d->setMinimumRowHeight(qMax(height, 0));
}

int QnWidgetTable::headerPadding() const
{
    Q_D(const QnWidgetTable);
    return d->headerPadding();
}

void QnWidgetTable::setHeaderPadding(int padding)
{
    Q_D(QnWidgetTable);
    d->setHeaderPadding(qMax(padding, 0));
}

bool QnWidgetTable::headerVisible() const
{
    Q_D(const QnWidgetTable);
    return d->headerVisible();
}

void QnWidgetTable::setHeaderVisible(bool visible)
{
    Q_D(QnWidgetTable);
    return d->setHeaderVisible(visible);
}

bool QnWidgetTable::sortingEnabled() const
{
    Q_D(const QnWidgetTable);
    return d->sortingEnabled();
}

void QnWidgetTable::setSortingEnabled(bool enable)
{
    Q_D(QnWidgetTable);
    d->setSortingEnabled(enable);
}

int QnWidgetTable::rowSpacing() const
{
    Q_D(const QnWidgetTable);
    return d->rowSpacing();
}

void QnWidgetTable::setRowSpacing(int spacing)
{
    Q_D(QnWidgetTable);
    d->setRowSpacing(qMax(spacing, 0));
}

QWidget* QnWidgetTable::widgetForCell(int row, int column) const
{
    Q_D(const QnWidgetTable);

    if (row < 0 || row >= d->rowCount())
        return nullptr;

    if (column < 0 || column >= d->columnCount())
        return nullptr;

    return d->widgetFor(row, column);
}
