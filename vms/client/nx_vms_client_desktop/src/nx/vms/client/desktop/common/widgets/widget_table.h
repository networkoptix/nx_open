#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QScrollArea>

class QHeaderView;

namespace nx::vms::client::desktop {

class WidgetTableDelegate;

/**
 * WidgetTable: a scrollable view into a simple item model displayed as a grid of widgets.
 * Creation of widgets, data exchange between them and the model and determining their size
 * is managed by a delegate of class WidgetTableDelegate.
 */
class WidgetTable: public QScrollArea
{
    Q_OBJECT
    using base_type = QScrollArea;

public:
    WidgetTable(QWidget* parent = nullptr);
    virtual ~WidgetTable();

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model, const QModelIndex& rootIndex = QModelIndex());

    QModelIndex rootIndex() const;
    void setRootIndex(const QModelIndex& rootIndex);

    /** Horizontal header. Ownership is taken permanently. */
    QHeaderView* header() const;
    void setHeader(QHeaderView* header);

    /** Item delegate. Ownership is not taken. Passing null restores default delegate. */
    WidgetTableDelegate* itemDelegate() const;
    void setItemDelegate(WidgetTableDelegate* newDelegate);

    /** Item delegate for particular column. Ownership is not taken. */
    WidgetTableDelegate* itemDelegateForColumn(int column) const;
    void setItemDelegateForColumn(int column, WidgetTableDelegate* newDelegate);

    int minimumRowHeight() const;
    void setMinimumRowHeight(int height);

    /** Margin between the header and the first row. */
    int headerPadding() const;
    void setHeaderPadding(int padding);

    bool headerVisible() const;
    void setHeaderVisible(bool visible);

    /** Whether sorting by clicking header sections is enabled. */
    bool sortingEnabled() const;
    void setSortingEnabled(bool enable);

    int rowSpacing() const;
    void setRowSpacing(int spacing);

    QWidget* widgetForCell(int row, int column) const;

    static QModelIndex indexForWidget(QWidget* widget);

    template<class T>
    inline T* widgetFor(int row, int column) const
    {
        return qobject_cast<T*>(widgetForCell(row, column));
    }

signals:
    /** Existing model was reset or a new model, root index or item delegate was set. */
    void tableReset();

private:
    using base_type::widget;
    using base_type::setWidget;
    using base_type::takeWidget;
    using base_type::widgetResizable;
    using base_type::setWidgetResizable;
    using base_type::setViewport;
    using base_type::alignment;
    using base_type::setAlignment;

private:
    Q_DISABLE_COPY(WidgetTable)
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
