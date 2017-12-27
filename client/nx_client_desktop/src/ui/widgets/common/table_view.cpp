#include "table_view.h"

#include <QtCore/QPointer>
#include <QtGui/QPainter>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QScrollBar>

#include <ui/common/item_view_hover_tracker.h>
#include <ui/style/helper.h>

QnTableView::QnTableView(QWidget* parent):
    base_type(parent),
    m_tracker(new QnItemViewHoverTracker(this))
{
    /* Make the defaults consistent with QTreeView: */
    horizontalHeader()->setHighlightSections(false);
    verticalHeader()->setHighlightSections(false);
}

QnTableView::~QnTableView()
{
}

QSize QnTableView::viewportSizeHint() const
{
    /* Fix for QTableView bug
     *  QTableView adjusts sizeHint if scrollBar.isVisible()
     *  But isVisible() deals with global visibility and returns false if QTableView is hidden (while its scrollbar is not)
     *  To check local visibility isHidden() must be used
     *  So as a workaround we adjust sizeHint after Qt if !scrollbar.isVisible() && !scrollbar.isHidden()
     *  It can happen during complicated layout recalculation, and sometimes scrollbar sizes are weird at this place.
     *  So we use style()->pixelMetric(QStyle::PM_ScrollBarExtent) as an upper size limit.
     */

    QSize size = base_type::viewportSizeHint();

    if (!verticalScrollBar()->isVisible() && !verticalScrollBar()->isHidden())
        size.rwidth() += qMin(verticalScrollBar()->width(), style()->pixelMetric(QStyle::PM_ScrollBarExtent));

    if (!horizontalScrollBar()->isVisible() && !horizontalScrollBar()->isHidden())
        size.rheight() += qMin(horizontalScrollBar()->height(), style()->pixelMetric(QStyle::PM_ScrollBarExtent));

    return size;
}

QnItemViewHoverTracker* QnTableView::hoverTracker() const
{
    return m_tracker;
}

bool QnTableView::edit(const QModelIndex& index, EditTrigger trigger, QEvent* event)
{
    if (trigger == QAbstractItemView::SelectedClicked && this->editTriggers().testFlag(QAbstractItemView::DoubleClicked))
        return base_type::edit(index, QAbstractItemView::DoubleClicked, event);

    return base_type::edit(index, trigger, event);
}

void QnTableView::paintEvent(QPaintEvent* event)
{
    if (selectionBehavior() == SelectRows)
    {
        QModelIndex current = currentIndex();
        if ((hasFocus() || viewport()->hasFocus()) && current.isValid())
        {
            /* Temporarily unfocus current item without sending any notifications: */
            QSignalBlocker blocker(selectionModel());
            selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
            /* Paint table: */
            base_type::paintEvent(event);
            /* Refocus previously focused item (still without sending any notifications): */
            selectionModel()->setCurrentIndex(current, QItemSelectionModel::NoUpdate);

            /* Paint entire row focus frame: */
            QPainter painter(viewport());
            QStyleOptionFocusRect frameOption;
            frameOption.initFrom(this);
            frameOption.rect = rowRect(current.row());
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &frameOption, &painter, this);

            /* Finish: */
            return;
        }
    }

    /* Default table painting: */
    base_type::paintEvent(event);
}

void QnTableView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    base_type::currentChanged(current, previous);

    /* Since we draw entire row focus rect if selection mode is SelectRows,
     * we need to post update events for rows that lose and gain focus: */
    if (selectionBehavior() == SelectRows && (hasFocus() || viewport()->hasFocus()))
    {
        int previousRow = previous.isValid() ? previous.row() : -1;
        int currentRow = current.isValid() ? current.row() : -1;

        if (previousRow != currentRow)
        {
            if (previousRow >= 0)
                viewport()->update(rowRect(previous.row()));

            if (currentRow >= 0)
                viewport()->update(rowRect(current.row()));
        }
    }
}

QRect QnTableView::rowRect(int row) const
{
    int lastColumn = horizontalHeader()->count() - 1;
    int rowStart = columnViewportPosition(0);
    int rowWidth = columnViewportPosition(lastColumn) + columnWidth(lastColumn) - rowStart;
    return QRect(rowStart, rowViewportPosition(row), rowWidth, rowHeight(row));
}

void QnTableView::setModel(QAbstractItemModel* newModel)
{
    const auto currentModel = model();
    if (currentModel == newModel)
        return;

    if (currentModel)
        disconnect(currentModel);

    base_type::setModel(newModel);

    if (!newModel)
        return;

    const auto openEditors =
        [this](int firstRow, int lastRow)
        {
            for (const auto column: m_delegates.keys())
                openEditorsForColumn(column, firstRow, lastRow);
        };

    connect(newModel, &QAbstractItemModel::rowsInserted, this,
        [openEditors](const QModelIndex& /*index*/, int firstRow, int lastRow)
        {
            openEditors(firstRow, lastRow);
        });

    const auto handleModelReset =
        [newModel, openEditors]()
        {
            openEditors(0, newModel->rowCount() - 1);
        };

    connect(newModel, &QAbstractItemModel::modelReset, this, handleModelReset);
    handleModelReset();
}

void QnTableView::setPersistentDelegateForColumn(int column, QAbstractItemDelegate* delegate)
{
    const auto it = m_delegates.find(column);
    if (it != m_delegates.end())
    {
        delete it.value().data();
        m_delegates.erase(it);
    }

    delegate->setParent(this);
    m_delegates.insert(column, QPointer<QAbstractItemDelegate>(delegate));

    setItemDelegateForColumn(column, delegate);
}

void QnTableView::openEditorsForColumn(int column, int firstRow, int lastRow)
{
    const auto currentModel = model();
    if (!currentModel)
        return;

    for (int row = firstRow; row <= lastRow; ++row)
        openPersistentEditor(currentModel->index(row, column));
}
