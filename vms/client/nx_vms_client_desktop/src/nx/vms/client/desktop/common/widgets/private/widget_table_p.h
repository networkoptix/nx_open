// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../widget_table.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QTimer>
#include <QtCore/QPointer>

namespace nx::vms::client::desktop {

class WidgetTable::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_DISABLE_COPY(Private)
    WidgetTable* const q;

public:
    Private(WidgetTable* q);

    QAbstractItemModel* model() const { return m_model; }
    void setModel(QAbstractItemModel* model, const QModelIndex& rootIndex);

    QModelIndex rootIndex() const { return m_rootIndex; }
    void setRootIndex(const QModelIndex& rootIndex, bool forceReset);

    QHeaderView* header() const;
    void setHeader(QHeaderView* newHeader);

    WidgetTableDelegate* commonDelegate() const;
    WidgetTableDelegate* columnDelegate(int column) const;

    /* Delegates are owned externally. WidgetTable does not take ownership. */
    void setUserDelegate(WidgetTableDelegate* newDelegate);
    void setColumnDelegate(int column, WidgetTableDelegate* newDelegate);

    int headerPadding() const { return m_headerPadding; }
    void setHeaderPadding(int padding);

    bool headerVisible() const;
    void setHeaderVisible(bool visible);

    bool sortingEnabled() const;
    void setSortingEnabled(bool enable);

    int rowSpacing() const { return m_rowSpacing; }
    void setRowSpacing(int spacing);

    int minimumRowHeight() const { return m_minimumRowHeight; }
    void setMinimumRowHeight(int height);

    void updateViewportMargins();
    int sizeHintForColumn(int column) const;

    int rowCount() const { return m_rows.size(); }
    int columnCount() const { return m_columnCount; }

    QWidget* widgetFor(int row, int column) const;

    // Connection between widget and model index. Index is stored with widget
    // as persistent model index and stays valid if model layout changes.
    static QModelIndex indexForWidget(QWidget* widget);
    static void setIndexForWidget(QWidget* widget, const QPersistentModelIndex& index);

private:
    void reset();
    void doLayout();
    void invalidateLayout();

    void updateSorting(int column, Qt::SortOrder order);

    void rowsInserted(const QModelIndex& parent, int first, int last);
    void rowsRemoved(const QModelIndex& parent, int first, int last);
    void rowsMoved(const QModelIndex& parent, int first, int last,
        const QModelIndex& newParent, int position);
    void columnsInserted(const QModelIndex& parent, int first, int last);
    void columnsRemoved(const QModelIndex& parent, int first, int last);
    void columnsMoved(const QModelIndex& parent, int first, int last,
        const QModelIndex& newParent, int position);
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
        const QVector<int>& roles);
    void layoutChanged(const QList<QPersistentModelIndex>& parents,
        QAbstractItemModel::LayoutChangeHint hint);

    void setWidgetFor(int row, int column, QWidget* newWidget);

    QWidget* createWidgetFor(int row, int column);
    void createWidgets(int firstRow, int lastRow, int firstColumn, int lastColumn);
    void cleanupWidgetFor(int row, int column);
    void cleanupWidgets(int firstRow, int lastRow, int firstColumn, int lastColumn);
    static void destroyWidget(QWidget* widget);

    void ensureWidgetVisible(QWidget* widget);

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QAbstractItemModel* m_model = nullptr;
    QPersistentModelIndex m_rootIndex;

    WidgetTableDelegate* const m_defaultDelegate;
    QPointer<WidgetTableDelegate> m_userDelegate;
    QHash<int, QPointer<WidgetTableDelegate>> m_columnDelegates;

    QWidget* const m_container;

    QTimer* const m_layoutTimer;

    class HeaderProxyView;
    HeaderProxyView* const m_headerProxyView;

    struct HorizontalRange;

    using Row = QVector<QPointer<QWidget>>;
    using Grid = QList<Row>;
    int m_columnCount = 0; //< To hold column count when row count is zero.
    Grid m_rows;

    int m_minimumRowHeight = 0;
    int m_headerPadding = 0;
    int m_rowSpacing = 0;
};

} // namespace nx::vms::client::desktop
