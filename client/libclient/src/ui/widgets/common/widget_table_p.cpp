#include "widget_table_p.h"
#include "widget_table_delegate.h"

#include <QtCore/QVarLengthArray>
#include <QtCore/private/qabstractitemmodel_p.h>

#include <QtWidgets/QApplication>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QHeaderView>

#include <utils/common/event_processors.h>

namespace {

/* Name of a widget property to hold QPesistentModelIndex bound to the widget: */
static const char* kModelIndexPropertyName = "_qn_widgetTableModelIndex";

} // namespace

/*
QnWidgetTablePrivate::HeaderProxyView
A private class - an empty tree view - showing only header
and delegating column size computations to our widget table
*/

class QnWidgetTablePrivate::HeaderProxyView: public QTreeView
{
    using base_type = QTreeView;

public:
    HeaderProxyView(QnWidgetTablePrivate* impl, QWidget* parent): base_type(parent), m_impl(impl)
    {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    virtual int sizeHintForColumn(int column) const override
    {
        return m_impl->sizeHintForColumn(column);
    }

    virtual void updateGeometries() override
    {
        base_type::updateGeometries();
        resize(width(), viewportMargins().top());
        m_impl->updateViewportMargins();
        m_impl->doLayout();
    }

private:
    QnWidgetTablePrivate* const m_impl;
};

/*
QnWidgetTablePrivate::HorizontalRange
*/

struct QnWidgetTablePrivate::HorizontalRange
{
    int left;
    int width;
    HorizontalRange(int left = 0, int width = 0): left(left), width(width) {}
};

/*
QnWidgetTablePrivate
*/

QnWidgetTablePrivate::QnWidgetTablePrivate(QnWidgetTable* table):
    base_type(),
    q_ptr(table),
    m_model(QAbstractItemModelPrivate::staticEmptyModel()),
    m_defaultDelegate(new QnWidgetTableDelegate(this)),
    m_container(new QWidget(table)),
    m_layoutTimer(new QTimer(this)),
    m_headerProxyView(new HeaderProxyView(this, table)),
    m_columnCount(0),
    m_minimumRowHeight(0),
    m_headerPadding(table->style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing)),
    m_rowSpacing(m_headerPadding)
{
    table->setWidget(m_container);
    table->setWidgetResizable(true);

    m_headerProxyView->ensurePolished();
    m_headerProxyView->setFrameShape(QFrame::NoFrame);
    m_headerProxyView->setContentsMargins(0, 0, 0, 0);
    m_headerProxyView->setPalette(table->palette());

    installEventHandler(m_container, { QEvent::Resize, QEvent::Move }, this,
        [this]()
        {
            /* Horizontal position and size are handled here: */
            const auto containerGeom = m_container->geometry();
            m_headerProxyView->setGeometry(containerGeom.left(), 0,
                containerGeom.width(), m_headerProxyView->height());
        });

    setHeader(new QHeaderView(Qt::Horizontal));

    m_layoutTimer->setSingleShot(true);
    connect(m_layoutTimer, &QTimer::timeout, this, &QnWidgetTablePrivate::doLayout);
}

void QnWidgetTablePrivate::setModel(QAbstractItemModel* model, const QModelIndex& rootIndex)
{
    if (!model)
        model = QAbstractItemModelPrivate::staticEmptyModel();

    if (model == m_model)
        return;

    if (m_model != QAbstractItemModelPrivate::staticEmptyModel())
        m_model->disconnect(this);

    m_model = model;
    header()->setModel(model);

    if (m_model != QAbstractItemModelPrivate::staticEmptyModel())
    {
        connect(m_model, &QAbstractItemModel::modelReset,
            this, &QnWidgetTablePrivate::reset);

        connect(m_model, &QAbstractItemModel::layoutChanged,
            this, &QnWidgetTablePrivate::layoutChanged);

        connect(m_model, &QAbstractItemModel::rowsInserted,
            this, &QnWidgetTablePrivate::rowsInserted);
        connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, &QnWidgetTablePrivate::rowsRemoved);
        connect(m_model, &QAbstractItemModel::rowsMoved,
            this, &QnWidgetTablePrivate::rowsMoved);

        connect(m_model, &QAbstractItemModel::columnsInserted,
            this, &QnWidgetTablePrivate::columnsInserted);
        connect(m_model, &QAbstractItemModel::columnsRemoved,
            this, &QnWidgetTablePrivate::columnsRemoved);
        connect(m_model, &QAbstractItemModel::columnsMoved,
            this, &QnWidgetTablePrivate::columnsMoved);

        connect(m_model, &QAbstractItemModel::dataChanged,
            this, &QnWidgetTablePrivate::dataChanged);
    }

    setRootIndex(rootIndex, true);

    updateSorting(header()->sortIndicatorSection(), header()->sortIndicatorOrder());
}

void QnWidgetTablePrivate::setRootIndex(const QModelIndex& rootIndex, bool forceReset)
{
    QModelIndex newRootIndex(rootIndex);
    if (newRootIndex.isValid() && newRootIndex.model() != m_model)
    {
        NX_ASSERT(false, Q_FUNC_INFO, "rootIndex should not be from another model");
        newRootIndex = QModelIndex();
    }

    if (newRootIndex == m_rootIndex && !forceReset)
        return;

    m_rootIndex = newRootIndex;
    reset();
}

QHeaderView* QnWidgetTablePrivate::header() const
{
    return m_headerProxyView->header();
}

void QnWidgetTablePrivate::setHeader(QHeaderView* newHeader)
{
    if (!newHeader || newHeader->orientation() != Qt::Horizontal)
        return;

    if (header() == newHeader)
        return;

    if (header())
        header()->disconnect(this);

    m_headerProxyView->setHeader(newHeader);
    newHeader->setModel(m_model);

    connect(newHeader, &QHeaderView::sectionResized, this, &QnWidgetTablePrivate::invalidateLayout);
    connect(newHeader, &QHeaderView::sectionMoved,   this, &QnWidgetTablePrivate::invalidateLayout);

    connect(newHeader, &QHeaderView::sortIndicatorChanged, this, &QnWidgetTablePrivate::updateSorting);
    updateSorting(newHeader->sortIndicatorSection(), newHeader->sortIndicatorOrder());
}

void QnWidgetTablePrivate::updateViewportMargins()
{
    Q_Q(QnWidgetTable);
    const int margin = (headerVisible() ? m_headerProxyView->height() + m_headerPadding : 0);
    q->setViewportMargins(0, margin, 0, 0);
}

void QnWidgetTablePrivate::setHeaderPadding(int padding)
{
    if (m_headerPadding == padding)
        return;

    m_headerPadding = padding;
    updateViewportMargins();
}

bool QnWidgetTablePrivate::headerVisible() const
{
    return !m_headerProxyView->isHidden();
}

void QnWidgetTablePrivate::setHeaderVisible(bool visible)
{
    if (visible == headerVisible())
        return;

    m_headerProxyView->setVisible(visible);
    updateViewportMargins();
}

bool QnWidgetTablePrivate::sortingEnabled() const
{
    return header()->isSortIndicatorShown() && header()->sectionsClickable();
}

void QnWidgetTablePrivate::setSortingEnabled(bool enable)
{
    if (sortingEnabled() == enable)
        return;

    const auto header = this->header();

    header->setSortIndicatorShown(enable);
    header->setSectionsClickable(enable);

    updateSorting(header->sortIndicatorSection(), header->sortIndicatorOrder());
}

void QnWidgetTablePrivate::setRowSpacing(int spacing)
{
    if (m_rowSpacing == spacing)
        return;

    m_rowSpacing = spacing;
    invalidateLayout();
}

void QnWidgetTablePrivate::setMinimumRowHeight(int height)
{
    if (m_minimumRowHeight == height)
        return;

    m_minimumRowHeight = height;
    invalidateLayout();
}

QnWidgetTableDelegate* QnWidgetTablePrivate::commonDelegate() const
{
    return (m_userDelegate ? m_userDelegate.data() : m_defaultDelegate);
}

QnWidgetTableDelegate* QnWidgetTablePrivate::columnDelegate(int column) const
{
    if (auto columnDelegate = m_columnDelegates[column])
        return columnDelegate;

    return commonDelegate();
}

void QnWidgetTablePrivate::setUserDelegate(QnWidgetTableDelegate* newDelegate)
{
    if (m_userDelegate == newDelegate)
        return;

    const auto oldEffectiveDelegate = commonDelegate();
    m_userDelegate = newDelegate;

    if (oldEffectiveDelegate == commonDelegate())
        return;

    reset();
}

void QnWidgetTablePrivate::setColumnDelegate(int column, QnWidgetTableDelegate* newDelegate)
{
    auto& columnDelegateRef = m_columnDelegates[column];
    if (columnDelegateRef == newDelegate)
        return;

    const auto oldEffectiveDelegate = columnDelegate(column);
    columnDelegateRef = newDelegate;

    if (oldEffectiveDelegate == columnDelegate(column))
        return;

    createWidgets(0, rowCount() - 1, column, column);
}

void QnWidgetTablePrivate::reset()
{
    cleanupWidgets(0, rowCount() - 1, 0, columnCount() - 1);
    m_rows.clear();

    const int columnCount = m_model->columnCount(m_rootIndex);
    const int rowCount = m_model->rowCount(m_rootIndex);

    constexpr int kRowsReserve = 32;
    m_rows.reserve(qMax(rowCount, kRowsReserve));

    m_columnCount = columnCount;

    if (rowCount > 0)
        rowsInserted(m_rootIndex, 0, rowCount - 1);

    Q_Q(QnWidgetTable);
    emit q->tableReset();
}

void QnWidgetTablePrivate::layoutChanged(
    const QList<QPersistentModelIndex>& parents,
    QAbstractItemModel::LayoutChangeHint hint)
{
    if (!parents.empty() && !parents.contains(m_rootIndex))
        return; //< nothing to do

    if (rowCount() == 0 || columnCount() == 0)
        return; //< nothing to do as well

    QVector<QWidget*> buffer; //< buffer for widgets relocation

    /* Copy widget pointer to the buffer and clear its source location: */
    auto fetchItem =
        [this, &buffer](QPointer<QWidget>& source)
        {
            if (source)
            {
                buffer.push_back(source);
                source = nullptr;
            }
        };

    /* Relocate widgets from the buffer to new locations in the grid: */
    auto relocateItems =
        [this, &buffer](const QRect& validRange) //< validRange is just for validation
        {
            for (auto widget: buffer)
            {
                const auto index = indexForWidget(widget);
                if (index.isValid())
                {
                    if (validRange.contains(index.column(), index.row()))
                    {
                        /* Relocate widget if index is valid: */
                        m_rows[index.row()][index.column()] = widget;
                    }
                    else
                    {
                        NX_ASSERT(false); //< should not happen with correct model
                        destroyWidget(widget);
                    }
                }
                else
                {
                    /* Destroy widget if index became invalid: */
                    destroyWidget(widget);
                }
            }
        };

    switch (hint)
    {
        case QAbstractItemModel::VerticalSortHint:
        {
            /* Relocate widgets within each column: */
            buffer.reserve(rowCount());
            QRect validRange(0, 0, 1, rowCount());

            for (int column = 0; column < columnCount(); ++column)
            {
                for (auto& row: m_rows)
                    fetchItem(row[column]);

                validRange.moveLeft(column); //< valid range is current column
                relocateItems(validRange);

                buffer.clear();
            }

            break;
        }

        case QAbstractItemModel::HorizontalSortHint:
        {
            /* Relocate widgets within each row: */
            buffer.reserve(columnCount());
            QRect validRange(0, 0, columnCount(), 1);

            for (int row = 0; row < rowCount(); ++row)
            {
                for (auto& item: m_rows[row])
                    fetchItem(item);

                validRange.moveTop(row); //< valid range is current row
                relocateItems(validRange);

                buffer.clear();
            }

            break;
        }

        default:
        {
            /* Relocate entire grid: */
            buffer.reserve(rowCount() * columnCount());
            const QRect validRange(0, 0, columnCount(), rowCount());

            for (auto& row: m_rows)
            {
                for (auto& item: row)
                    fetchItem(item);
            }

            relocateItems(validRange); //< valid range is entire grid
            break;
        }
    }

    invalidateLayout();
}

void QnWidgetTablePrivate::rowsInserted(const QModelIndex& parent, int first, int last)
{
    if (parent != m_rootIndex)
        return;

    const Row toInsert(columnCount());

    for (int row = first; row <= last; ++row)
        m_rows.insert(row, toInsert);

    createWidgets(first, last, 0, columnCount() - 1);
    invalidateLayout();
}

void QnWidgetTablePrivate::rowsRemoved(const QModelIndex& parent, int first, int last)
{
    if (parent != m_rootIndex)
        return;

    cleanupWidgets(first, last, 0, columnCount() - 1);

    m_rows.erase(m_rows.begin() + first, m_rows.begin() + last + 1);
    invalidateLayout();
}

void QnWidgetTablePrivate::rowsMoved(const QModelIndex& parent, int first, int last,
    const QModelIndex& newParent, int position)
{
    if (parent != newParent)
    {
        rowsRemoved(parent, first, last);
        rowsInserted(newParent, position, position + last - first);
        return;
    }

    if (parent != m_rootIndex)
        return;

    const int end = last + 1;

    if (position < first)
        std::rotate(m_rows.begin() + position, m_rows.begin() + first, m_rows.begin() + end);
    else if (position > end)
        std::rotate(m_rows.begin() + first, m_rows.begin() + end, m_rows.begin() + position);

    invalidateLayout();
}

void QnWidgetTablePrivate::columnsInserted(const QModelIndex& parent, int first, int last)
{
    if (parent != m_rootIndex)
        return;

    const int count = last - first + 1;

    for (auto& row: m_rows)
        row.insert(row.begin() + first, count, nullptr);

    m_columnCount += count;

    createWidgets(0, rowCount() - 1, first, last);
    invalidateLayout();
}

void QnWidgetTablePrivate::columnsRemoved(const QModelIndex& parent, int first, int last)
{
    if (parent != m_rootIndex)
        return;

    const int afterLast = last + 1;

    cleanupWidgets(0, rowCount() - 1, first, last);

    for (auto& row: m_rows)
        row.erase(row.begin() + first, row.begin() + afterLast);

    m_columnCount -= (afterLast - first);
    invalidateLayout();
}

void QnWidgetTablePrivate::columnsMoved(const QModelIndex& parent, int first, int last,
    const QModelIndex& newParent, int position)
{
    if (parent != newParent)
    {
        columnsRemoved(parent, first, last);
        columnsInserted(newParent, position, position + last - first);
        return;
    }

    if (parent != m_rootIndex)
        return;

    const int end = last + 1;

    if (position < first)
    {
        for (auto& row: m_rows)
            std::rotate(row.begin() + position, row.begin() + first, row.begin() + end);
    }
    else if (position > end)
    {
        for (auto& row: m_rows)
            std::rotate(row.begin() + first, row.begin() + end, row.begin() + position);
    }

    invalidateLayout();
}

void QnWidgetTablePrivate::dataChanged(
    const QModelIndex& topLeft,
    const QModelIndex& bottomRight,
    const QVector<int>& roles)
{
    Q_UNUSED(roles); //< unused so far

    if (topLeft.parent() != bottomRight.parent())
        return;

    if (topLeft.parent() != m_rootIndex)
        return;

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        for (int column = topLeft.column(); column <= bottomRight.column(); ++column)
        {
            const auto widget = widgetFor(row, column);
            const auto index = m_model->index(row, column, m_rootIndex);

            if (!widget || !columnDelegate(column)->updateWidget(widget, index))
            {
                if (createWidgetFor(row, column) != widget)
                    invalidateLayout();
            }
        }
    }
}

QWidget* QnWidgetTablePrivate::widgetFor(int row, int column) const
{
    return m_rows[row][column];
}

void QnWidgetTablePrivate::setWidgetFor(int row, int column, QWidget* newWidget)
{
    auto& widget = m_rows[row][column];
    if (widget == newWidget)
        return;

    destroyWidget(widget); //< has nullptr check inside
    widget = newWidget;

    NX_EXPECT(!widget || widget->parent() == m_container);
}

QWidget* QnWidgetTablePrivate::createWidgetFor(int row, int column)
{
    const auto index = m_model->index(row, column, m_rootIndex);
    auto itemDelegate = columnDelegate(column);

    auto widget = itemDelegate->createWidget(m_model, index, m_container);
    if (widget)
    {
        widget->installEventFilter(this);
        setIndexForWidget(widget, index);
        itemDelegate->updateWidget(widget, index);
    }

    setWidgetFor(row, column, widget);
    return widget;
}

void QnWidgetTablePrivate::createWidgets(int firstRow, int lastRow, int firstColumn, int lastColumn)
{
    for (int row = firstRow; row <= lastRow; ++row)
    {
        for (int column = firstColumn; column <= lastColumn; ++column)
            createWidgetFor(row, column);
    }
}

void QnWidgetTablePrivate::cleanupWidgets(int firstRow, int lastRow, int firstColumn, int lastColumn)
{
    for (int row = firstRow; row <= lastRow; ++row)
    {
        for (int column = firstColumn; column <= lastColumn; ++column)
            cleanupWidgetFor(row, column);
    }
}

void QnWidgetTablePrivate::cleanupWidgetFor(int row, int column)
{
    setWidgetFor(row, column, nullptr);
}

void QnWidgetTablePrivate::destroyWidget(QWidget* widget)
{
    if (!widget)
        return;

    setIndexForWidget(widget, QModelIndex());
    widget->setHidden(true);
    widget->deleteLater();
}

int QnWidgetTablePrivate::sizeHintForColumn(int column) const
{
    if (column < 0 || column >= columnCount())
        return 0;

    int width = 0;
    for (int row = 0; row < rowCount(); ++row)
    {
        const auto widget = widgetFor(row, column);
        if (!widget)
            continue;

        const auto index = m_model->index(row, column);
        const auto itemDelegate = columnDelegate(column);
        const auto indents = itemDelegate->itemIndents(widget, index);
        width = qMax(width, itemDelegate->sizeHint(widget, index).width()
            + indents.left() + indents.right());
    }

    return width;
}

void QnWidgetTablePrivate::updateSorting(int column, Qt::SortOrder order)
{
    if (sortingEnabled())
        m_model->sort(column, order);
}

void QnWidgetTablePrivate::invalidateLayout()
{
    if (m_layoutTimer->isActive())
        return;

    m_layoutTimer->start(1);
}

void QnWidgetTablePrivate::doLayout()
{
    /* Column locations: position and width: */
    QVarLengthArray<HorizontalRange> columnBounds(columnCount());

    const auto header = this->header();
    const int headerSectionCount = header->count();

    /* Fetch column bounds from the header: */
    const int count = qMin(columnCount(), headerSectionCount); //< to be safe
    for (int i = 0; i < count; ++i)
    {
        if (header->isSectionHidden(i))
            continue;

        columnBounds[i] = HorizontalRange(
            header->sectionViewportPosition(i),
            header->sectionSize(i));
    }

    /* Calculated item geometries, per row: */
    QVarLengthArray<QRect> itemRects(columnCount());

    /* Visual-to-logical column lookup: */
    QVarLengthArray<int> visualToLogical(columnCount());
    for (int i = 0; i < columnCount(); ++i)
        visualToLogical[i] = (i < headerSectionCount ? header->logicalIndex(i) : i);

    int y = 0;
    QWidget* previousInFocusChain = nullptr;

    /* Process rows: */
    for (int row = 0; row < rowCount(); ++row)
    {
        /* Calculate cell geometries: */
        int maxHeight = m_minimumRowHeight;
        for (int visualColumn = 0; visualColumn < columnCount(); ++visualColumn)
        {
            const int column = visualToLogical[visualColumn];
            const auto widget = widgetFor(row, column);
            if (!widget)
                continue;

            /* Maintain focus chain: */
            if (widget->focusPolicy() != Qt::NoFocus)
            {
                if (previousInFocusChain)
                    QWidget::setTabOrder(previousInFocusChain, widget);

                previousInFocusChain = widget;
            }

            const auto& bounds = columnBounds[column];
            bool hidden = (bounds.width == 0);

            widget->setHidden(hidden);
            if (hidden)
                continue;

            const auto index = m_model->index(row, column, m_rootIndex);
            const auto itemDelegate = columnDelegate(column);
            const auto indents = itemDelegate->itemIndents(widget, index);
            const auto sizeHint = itemDelegate->sizeHint(widget, index);

            itemRects[column] = QRect(
                bounds.left + indents.left(),
                y,
                bounds.width - indents.left() - indents.right(),
                sizeHint.height());

            maxHeight = qMax(maxHeight, sizeHint.height());
        }

        /* Apply row geometries: */
        for (int column = 0; column < columnCount(); ++column)
        {
            const auto widget = widgetFor(row, column);
            if (!widget || widget->isHidden())
                continue;

            const auto contentRect = itemRects[column];
            const auto contentSize = contentRect.size();

            const auto itemRect = QRect(
                contentRect.left(),
                contentRect.top(),
                contentRect.width(),
                maxHeight);

            widget->setGeometry(QStyle::alignedRect(
                Qt::LeftToRight, Qt::AlignCenter,
                contentRect.size(),
                itemRect));
        }

        y += maxHeight + m_rowSpacing;
    }

    m_container->setMinimumHeight(y);

    /* Keep widget with focus visible. All safety checks are inside. */
    ensureWidgetVisible(qApp->focusWidget());
}

void QnWidgetTablePrivate::ensureWidgetVisible(QWidget* widget)
{
    if (!widget || !indexForWidget(widget).isValid())
        return;

    Q_Q(QnWidgetTable);
    q->ensureWidgetVisible(widget);
}

bool QnWidgetTablePrivate::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::FocusIn:
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
            ensureWidgetVisible(qobject_cast<QWidget*>(watched)); //< safety checks are inside
            break;

        default:
            break;
    }

    return base_type::eventFilter(watched, event);
}

QModelIndex QnWidgetTablePrivate::indexForWidget(QWidget* widget)
{
    return widget
        ? widget->property(kModelIndexPropertyName).toModelIndex()
        : QModelIndex();
}

void QnWidgetTablePrivate::setIndexForWidget(QWidget* widget, const QPersistentModelIndex& index)
{
    if (widget)
        widget->setProperty(kModelIndexPropertyName, index);
}
