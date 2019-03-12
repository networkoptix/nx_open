#include "widget_table_p.h"

#include <QtCore/QVarLengthArray>
#include <QtCore/private/qabstractitemmodel_p.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QHeaderView>

#include <utils/common/event_processors.h>

#include <nx/vms/client/desktop/common/delegates/widget_table_delegate.h>

namespace nx::vms::client::desktop {

namespace {

// Name of a widget property to hold QPesistentModelIndex bound to the widget.
static const char* kModelIndexPropertyName = "_qn_widgetTableModelIndex";

} // namespace

// ------------------------------------------------------------------------------------------------
// WidgetTable::Private::HeaderProxyView
// A private class - an empty tree view - showing only header
// and delegating column size computations to our widget table

class WidgetTable::Private::HeaderProxyView: public QTreeView
{
    using base_type = QTreeView;

public:
    HeaderProxyView(WidgetTable::Private* impl, QWidget* parent): base_type(parent), m_impl(impl)
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
    WidgetTable::Private* const m_impl;
};

// ------------------------------------------------------------------------------------------------
// WidgetTable::Private::HorizontalRange

struct WidgetTable::Private::HorizontalRange
{
    int left = 0;
    int width = 0;
    HorizontalRange(int left = 0, int width = 0): left(left), width(width) {}
};

// ------------------------------------------------------------------------------------------------
// WidgetTable::Private

WidgetTable::Private::Private(WidgetTable* q):
    base_type(),
    q(q),
    m_model(QAbstractItemModelPrivate::staticEmptyModel()),
    m_defaultDelegate(new WidgetTableDelegate(this)),
    m_container(new QWidget(q)),
    m_layoutTimer(new QTimer(this)),
    m_headerProxyView(new HeaderProxyView(this, q)),
    m_headerPadding(q->style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing)),
    m_rowSpacing(m_headerPadding)
{
    q->setWidget(m_container);
    q->setWidgetResizable(true);

    m_headerProxyView->ensurePolished();
    m_headerProxyView->setFrameShape(QFrame::NoFrame);
    m_headerProxyView->setContentsMargins(0, 0, 0, 0);
    m_headerProxyView->setPalette(q->palette());

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
    connect(m_layoutTimer, &QTimer::timeout, this, &WidgetTable::Private::doLayout);
}

void WidgetTable::Private::setModel(QAbstractItemModel* model, const QModelIndex& rootIndex)
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
            this, &WidgetTable::Private::reset);

        connect(m_model, &QAbstractItemModel::layoutChanged,
            this, &WidgetTable::Private::layoutChanged);

        connect(m_model, &QAbstractItemModel::rowsInserted,
            this, &WidgetTable::Private::rowsInserted);
        connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, &WidgetTable::Private::rowsRemoved);
        connect(m_model, &QAbstractItemModel::rowsMoved,
            this, &WidgetTable::Private::rowsMoved);

        connect(m_model, &QAbstractItemModel::columnsInserted,
            this, &WidgetTable::Private::columnsInserted);
        connect(m_model, &QAbstractItemModel::columnsRemoved,
            this, &WidgetTable::Private::columnsRemoved);
        connect(m_model, &QAbstractItemModel::columnsMoved,
            this, &WidgetTable::Private::columnsMoved);

        connect(m_model, &QAbstractItemModel::dataChanged,
            this, &WidgetTable::Private::dataChanged);
    }

    setRootIndex(rootIndex, true);

    updateSorting(header()->sortIndicatorSection(), header()->sortIndicatorOrder());
}

void WidgetTable::Private::setRootIndex(const QModelIndex& rootIndex, bool forceReset)
{
    QModelIndex newRootIndex(rootIndex);
    if (newRootIndex.isValid() && newRootIndex.model() != m_model)
    {
        NX_ASSERT(false, "rootIndex should not be from another model");
        newRootIndex = QModelIndex();
    }

    if (newRootIndex == m_rootIndex && !forceReset)
        return;

    m_rootIndex = newRootIndex;
    reset();
}

QHeaderView* WidgetTable::Private::header() const
{
    return m_headerProxyView->header();
}

void WidgetTable::Private::setHeader(QHeaderView* newHeader)
{
    if (!newHeader || newHeader->orientation() != Qt::Horizontal)
        return;

    if (header() == newHeader)
        return;

    if (header())
        header()->disconnect(this);

    m_headerProxyView->setHeader(newHeader);
    newHeader->setModel(m_model);

    connect(newHeader, &QHeaderView::sectionResized, this, &WidgetTable::Private::invalidateLayout);
    connect(newHeader, &QHeaderView::sectionMoved,   this, &WidgetTable::Private::invalidateLayout);

    connect(newHeader, &QHeaderView::sortIndicatorChanged, this, &WidgetTable::Private::updateSorting);
    updateSorting(newHeader->sortIndicatorSection(), newHeader->sortIndicatorOrder());
}

void WidgetTable::Private::updateViewportMargins()
{
    const int margin = (headerVisible() ? m_headerProxyView->height() + m_headerPadding : 0);
    q->setViewportMargins(0, margin, 0, 0);
}

void WidgetTable::Private::setHeaderPadding(int padding)
{
    if (m_headerPadding == padding)
        return;

    m_headerPadding = padding;
    updateViewportMargins();
}

bool WidgetTable::Private::headerVisible() const
{
    return !m_headerProxyView->isHidden();
}

void WidgetTable::Private::setHeaderVisible(bool visible)
{
    if (visible == headerVisible())
        return;

    m_headerProxyView->setVisible(visible);
    updateViewportMargins();
}

bool WidgetTable::Private::sortingEnabled() const
{
    return header()->isSortIndicatorShown() && header()->sectionsClickable();
}

void WidgetTable::Private::setSortingEnabled(bool enable)
{
    if (sortingEnabled() == enable)
        return;

    const auto header = this->header();

    header->setSortIndicatorShown(enable);
    header->setSectionsClickable(enable);

    updateSorting(header->sortIndicatorSection(), header->sortIndicatorOrder());
}

void WidgetTable::Private::setRowSpacing(int spacing)
{
    if (m_rowSpacing == spacing)
        return;

    m_rowSpacing = spacing;
    invalidateLayout();
}

void WidgetTable::Private::setMinimumRowHeight(int height)
{
    if (m_minimumRowHeight == height)
        return;

    m_minimumRowHeight = height;
    invalidateLayout();
}

WidgetTableDelegate* WidgetTable::Private::commonDelegate() const
{
    return (m_userDelegate ? m_userDelegate.data() : m_defaultDelegate);
}

WidgetTableDelegate* WidgetTable::Private::columnDelegate(int column) const
{
    if (auto columnDelegate = m_columnDelegates[column])
        return columnDelegate;

    return commonDelegate();
}

void WidgetTable::Private::setUserDelegate(WidgetTableDelegate* newDelegate)
{
    if (m_userDelegate == newDelegate)
        return;

    const auto oldEffectiveDelegate = commonDelegate();
    m_userDelegate = newDelegate;

    if (oldEffectiveDelegate == commonDelegate())
        return;

    reset();
}

void WidgetTable::Private::setColumnDelegate(int column, WidgetTableDelegate* newDelegate)
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

void WidgetTable::Private::reset()
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

    emit q->tableReset();
}

void WidgetTable::Private::layoutChanged(
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

void WidgetTable::Private::rowsInserted(const QModelIndex& parent, int first, int last)
{
    if (parent != m_rootIndex)
        return;

    const Row toInsert(columnCount());

    for (int row = first; row <= last; ++row)
        m_rows.insert(row, toInsert);

    createWidgets(first, last, 0, columnCount() - 1);
    invalidateLayout();
}

void WidgetTable::Private::rowsRemoved(const QModelIndex& parent, int first, int last)
{
    if (parent != m_rootIndex)
        return;

    cleanupWidgets(first, last, 0, columnCount() - 1);

    m_rows.erase(m_rows.begin() + first, m_rows.begin() + last + 1);
    invalidateLayout();
}

void WidgetTable::Private::rowsMoved(const QModelIndex& parent, int first, int last,
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

void WidgetTable::Private::columnsInserted(const QModelIndex& parent, int first, int last)
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

void WidgetTable::Private::columnsRemoved(const QModelIndex& parent, int first, int last)
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

void WidgetTable::Private::columnsMoved(const QModelIndex& parent, int first, int last,
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

void WidgetTable::Private::dataChanged(
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

QWidget* WidgetTable::Private::widgetFor(int row, int column) const
{
    return m_rows[row][column];
}

void WidgetTable::Private::setWidgetFor(int row, int column, QWidget* newWidget)
{
    auto& widget = m_rows[row][column];
    if (widget == newWidget)
        return;

    destroyWidget(widget); //< has nullptr check inside
    widget = newWidget;

    NX_ASSERT(!widget || widget->parent() == m_container);
}

QWidget* WidgetTable::Private::createWidgetFor(int row, int column)
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

void WidgetTable::Private::createWidgets(int firstRow, int lastRow, int firstColumn, int lastColumn)
{
    for (int row = firstRow; row <= lastRow; ++row)
    {
        for (int column = firstColumn; column <= lastColumn; ++column)
            createWidgetFor(row, column);
    }
}

void WidgetTable::Private::cleanupWidgets(int firstRow, int lastRow, int firstColumn, int lastColumn)
{
    for (int row = firstRow; row <= lastRow; ++row)
    {
        for (int column = firstColumn; column <= lastColumn; ++column)
            cleanupWidgetFor(row, column);
    }
}

void WidgetTable::Private::cleanupWidgetFor(int row, int column)
{
    setWidgetFor(row, column, nullptr);
}

void WidgetTable::Private::destroyWidget(QWidget* widget)
{
    if (!widget)
        return;

    setIndexForWidget(widget, QModelIndex());
    widget->setHidden(true);
    widget->deleteLater();
}

int WidgetTable::Private::sizeHintForColumn(int column) const
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

void WidgetTable::Private::updateSorting(int column, Qt::SortOrder order)
{
    if (sortingEnabled())
        m_model->sort(column, order);
}

void WidgetTable::Private::invalidateLayout()
{
    if (m_layoutTimer->isActive())
        return;

    m_layoutTimer->start(1);
}

void WidgetTable::Private::doLayout()
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

void WidgetTable::Private::ensureWidgetVisible(QWidget* widget)
{
    if (!widget || !indexForWidget(widget).isValid())
        return;

    q->ensureWidgetVisible(widget);
}

bool WidgetTable::Private::eventFilter(QObject* watched, QEvent* event)
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

QModelIndex WidgetTable::Private::indexForWidget(QWidget* widget)
{
    return widget
        ? widget->property(kModelIndexPropertyName).toModelIndex()
        : QModelIndex();
}

void WidgetTable::Private::setIndexForWidget(QWidget* widget, const QPersistentModelIndex& index)
{
    if (widget)
        widget->setProperty(kModelIndexPropertyName, index);
}

} // namespace nx::vms::client::desktop
