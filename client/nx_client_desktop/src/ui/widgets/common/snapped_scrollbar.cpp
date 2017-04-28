#include "snapped_scrollbar.h"
#include "private/snapped_scrollbar_p.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QHeaderView>

namespace
{
    QSize headerSize(QWidget *widget, Qt::Orientation orientation)
    {
        QAbstractItemView *itemView = nullptr;

        while (!itemView && widget)
        {
            itemView = qobject_cast<QAbstractItemView *>(widget);
            widget = widget->parentWidget();
        }

        if (!itemView)
            return QSize();

        if (orientation == Qt::Horizontal)
        {
            if (const QTableView *table = qobject_cast<const QTableView *>(itemView))
                return table->horizontalHeader()->size();
            else if (const QTreeView *tree = qobject_cast<const QTreeView *>(itemView))
                return tree->header()->size();
        }
        else
        {
            if (const QTableView *table = qobject_cast<const QTableView *>(itemView))
                return table->verticalHeader()->size();
        }
        return QSize();
    }
}

QnSnappedScrollBar::QnSnappedScrollBar(QWidget *parent)
    : base_type(Qt::Vertical, parent),
      d_ptr(new QnSnappedScrollBarPrivate(this))
{
}

QnSnappedScrollBar::QnSnappedScrollBar(Qt::Orientation orientation, QWidget *parent)
    : base_type(orientation, parent),
      d_ptr(new QnSnappedScrollBarPrivate(this))
{
}

QnSnappedScrollBar::~QnSnappedScrollBar()
{
}

Qt::Alignment QnSnappedScrollBar::alignment() const
{
    Q_D(const QnSnappedScrollBar);
    return d->alignment;
}

void QnSnappedScrollBar::setAlignment(Qt::Alignment alignment)
{
    Q_D(QnSnappedScrollBar);

    if (d->alignment == alignment)
        return;

    d->alignment = alignment;
    updateGeometry();
}

bool QnSnappedScrollBar::useHeaderShift() const
{
    Q_D(const QnSnappedScrollBar);
    return d->useHeaderShift;
}

void QnSnappedScrollBar::setUseHeaderShift(bool useHeaderShift)
{
    Q_D(QnSnappedScrollBar);

    if (d->useHeaderShift == useHeaderShift)
        return;

    d->useHeaderShift = useHeaderShift;
    d->updateGeometry();
}

bool QnSnappedScrollBar::useItemViewPaddingWhenVisible() const
{
    Q_D(const QnSnappedScrollBar);
    return d->useItemViewPaddingWhenVisible;
}

void QnSnappedScrollBar::setUseItemViewPaddingWhenVisible(bool useItemViewPaddingWhenVisible)
{
    Q_D(QnSnappedScrollBar);
    if (d->useItemViewPaddingWhenVisible == useItemViewPaddingWhenVisible)
        return;

    d->useItemViewPaddingWhenVisible = useItemViewPaddingWhenVisible;
    d->updateProxyScrollBarSize();
}

bool QnSnappedScrollBar::useMaximumSpace() const
{
    Q_D(const QnSnappedScrollBar);
    return d->useMaximumSpace;
}

void QnSnappedScrollBar::setUseMaximumSpace(bool useMaximumSpace)
{
    Q_D(QnSnappedScrollBar);
    if (d->useMaximumSpace == useMaximumSpace)
        return;

    d->useMaximumSpace = useMaximumSpace;
    d->updateGeometry();
}

QnScrollBarProxy *QnSnappedScrollBar::proxyScrollBar() const
{
    Q_D(const QnSnappedScrollBar);
    return d->proxyScrollbar;
}

QnSnappedScrollBarPrivate::QnSnappedScrollBarPrivate(QnSnappedScrollBar *parent) :
    q_ptr(parent),
    alignment(Qt::AlignRight | Qt::AlignBottom),
    useHeaderShift(true),
    useItemViewPaddingWhenVisible(false),
    useMaximumSpace(false)
{
    Q_Q(QnSnappedScrollBar);

    proxyScrollbar = new QnScrollBarProxy(q, q);

    if (q->parentWidget())
        q->parentWidget()->installEventFilter(this);

    proxyScrollbar->installEventFilter(this);
    q->installEventFilter(this);
}

void QnSnappedScrollBarPrivate::updateGeometry()
{
    Q_Q(QnSnappedScrollBar);

    QWidget *parent = q->parentWidget();

    if (!parent)
        return;

    if (!proxyScrollbar || !proxyScrollbar->parentWidget())
        return;

    QRect parentRect = parent->rect();

    QPoint pos = useMaximumSpace ? parentRect.topLeft() : proxyScrollbar->mapTo(parent, proxyScrollbar->rect().topLeft());
    QRect geometry = QRect(pos, q->sizeHint());

    if (q->orientation() == Qt::Vertical)
    {
        if (useMaximumSpace)
            geometry.setHeight(parentRect.height());
        else
            geometry.setHeight(proxyScrollbar->height());

        if (alignment.testFlag(Qt::AlignRight))
            geometry.moveRight(parentRect.right());
        else if (alignment.testFlag(Qt::AlignLeft))
            geometry.moveLeft(parentRect.left());

        if (useHeaderShift)
            geometry.setTop(geometry.top() + headerSize(proxyScrollbar->parentWidget(), Qt::Horizontal).height());
    }
    else
    {
        if (useMaximumSpace)
            geometry.setWidth(parentRect.width());
        else
            geometry.setWidth(proxyScrollbar->width());

        if (alignment.testFlag(Qt::AlignBottom))
            geometry.moveBottom(parentRect.bottom());
        else if (alignment.testFlag(Qt::AlignTop))
            geometry.moveTop(parentRect.top());

        if (useHeaderShift)
            geometry.setLeft(geometry.left() + headerSize(proxyScrollbar->parentWidget(), Qt::Vertical).width());
    }

    q->setGeometry(geometry);
}

void QnSnappedScrollBarPrivate::updateProxyScrollBarSize()
{
    Q_Q(QnSnappedScrollBar);
    if (!proxyScrollbar)
        return;

    if (!useItemViewPaddingWhenVisible)
    {
        proxyScrollbar->setMinimumSize(QSize(1, 1));
        return;
    }

    bool usePadding = q->isVisible();
    int minSize = usePadding ? qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent) : 1;

    if (q->orientation() == Qt::Vertical)
        proxyScrollbar->setMinimumWidth(minSize);
    else
        proxyScrollbar->setMinimumHeight(minSize);
}

bool QnSnappedScrollBarPrivate::eventFilter(QObject *object, QEvent *event)
{
    Q_Q(QnSnappedScrollBar);
    if (!proxyScrollbar)
        return false;

    if (object == q->parentWidget())
    {
        if (event->type() == QEvent::Resize)
            updateGeometry();
    }
    else if (object == proxyScrollbar->parentWidget() && !useMaximumSpace)
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move)
            updateGeometry();
    }
    else if (object == proxyScrollbar)
    {
        switch (event->type())
        {
        case QEvent::ParentAboutToChange:
            proxyScrollbar->parentWidget()->removeEventFilter(this);
            break;
        case QEvent::ParentChange:
            proxyScrollbar->parentWidget()->installEventFilter(this);
            updateProxyScrollBarSize();
            break;
        default:
            break;
        }
    }
    else if (object == q)
    {
        switch (event->type())
        {
        case QEvent::ParentAboutToChange:
            q->parentWidget()->removeEventFilter(this);
            break;
        case QEvent::ParentChange:
            q->parentWidget()->installEventFilter(this);
            updateProxyScrollBarSize();
            break;
        case QEvent::Hide:
        case QEvent::Show:
            updateProxyScrollBarSize();
            break;
        default:
            break;
        }
    }

    return false;
}
