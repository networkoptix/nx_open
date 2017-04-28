#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QScrollArea>

class QHeaderView;
class QnWidgetTableDelegate;
class QnWidgetTablePrivate;

/*
* QnWidgetTable: a scrollable view into a simple item model displayed as a grid of widgets.
* Creation of widgets, data exchange between them and the model and determining their size
* is managed by a delegate of class QnWidgetTableDelegate.
*/
class QnWidgetTable: public QScrollArea
{
    Q_OBJECT
    using base_type = QScrollArea;

public:
    /** Construction/destruction: */
    QnWidgetTable(QWidget* parent = nullptr);
    virtual ~QnWidgetTable();

    /** Item model to view/edit: */
    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model, const QModelIndex& rootIndex = QModelIndex());

    /** Root index in the model: */
    QModelIndex rootIndex() const;
    void setRootIndex(const QModelIndex& rootIndex);

    /** Horizontal header. Ownership is taken permanently: */
    QHeaderView* header() const;
    void setHeader(QHeaderView* header);

    /** Item delegate. Ownership is not taken.
      * Passing nullptr restores default delegate: */
    QnWidgetTableDelegate* itemDelegate() const;
    void setItemDelegate(QnWidgetTableDelegate* newDelegate);

    /** Item delegate for particular column. Ownership is not taken: */
    QnWidgetTableDelegate* itemDelegateForColumn(int column) const;
    void setItemDelegateForColumn(int column, QnWidgetTableDelegate* newDelegate);

    /** Minimim row height: */
    int minimumRowHeight() const;
    void setMinimumRowHeight(int height);

    /** Margin between the header and the first row: */
    int headerPadding() const;
    void setHeaderPadding(int padding);

    /** Header visibility: */
    bool headerVisible() const;
    void setHeaderVisible(bool visible);

    /** If sorting by clicking header sections is enabled: */
    bool sortingEnabled() const;
    void setSortingEnabled(bool enable);

    /** Spacing between rows: */
    int rowSpacing() const;
    void setRowSpacing(int spacing);

    /** Widget for specified cell: */
    QWidget* widgetForCell(int row, int column) const;

    template<class T>
    inline T* widgetFor(int row, int column) const
    {
        return qobject_cast<T*>(widgetForCell(row, column));
    }

signals:
    /** Existing model was reset or a new model, root index or item delegate was set: */
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
    Q_DISABLE_COPY(QnWidgetTable)
    Q_DECLARE_PRIVATE(QnWidgetTable)
    const QScopedPointer<QnWidgetTablePrivate> d_ptr;
};
