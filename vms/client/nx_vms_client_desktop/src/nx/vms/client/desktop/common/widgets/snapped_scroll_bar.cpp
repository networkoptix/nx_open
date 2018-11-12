#include "snapped_scroll_bar.h"

#include <QtCore/QPointer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// SnappedScrollBar::Private

class SnappedScrollBar::Private: public QObject
{
    SnappedScrollBar* const q;

public:
    Private(SnappedScrollBar* q);

    void updateGeometry();
    void updateProxyScrollBarSize();

    bool eventFilter(QObject* object, QEvent* event);

public:
    QPointer<ScrollBarProxy> proxyScrollbar;
    Qt::Alignment alignment = Qt::AlignRight | Qt::AlignBottom;
    bool useItemViewPaddingWhenVisible = false;
    bool useMaximumSpace = false;
};

SnappedScrollBar::Private::Private(SnappedScrollBar* q):
    q(q),
    proxyScrollbar(new ScrollBarProxy(q, q))
{
    if (q->parentWidget())
        q->parentWidget()->installEventFilter(this);

    proxyScrollbar->installEventFilter(this);
    q->installEventFilter(this);
}

void SnappedScrollBar::Private::updateGeometry()
{
    const auto parent = q->parentWidget();
    if (!parent)
        return;

    if (!proxyScrollbar || !proxyScrollbar->parentWidget())
        return;

    QRect parentRect = parent->rect();

    const QPoint pos = useMaximumSpace
        ? parentRect.topLeft()
        : proxyScrollbar->mapTo(parent, proxyScrollbar->rect().topLeft());

    QRect geometry = QRect(pos, q->sizeHint());

    if (q->orientation() == Qt::Vertical)
    {
        geometry.setHeight(useMaximumSpace ? parentRect.height() : proxyScrollbar->height());

        if (alignment.testFlag(Qt::AlignRight))
            geometry.moveRight(parentRect.right());
        else if (alignment.testFlag(Qt::AlignLeft))
            geometry.moveLeft(parentRect.left());
    }
    else
    {
        geometry.setWidth(useMaximumSpace ? parentRect.width() : proxyScrollbar->width());

        if (alignment.testFlag(Qt::AlignBottom))
            geometry.moveBottom(parentRect.bottom());
        else if (alignment.testFlag(Qt::AlignTop))
            geometry.moveTop(parentRect.top());
    }

    q->setGeometry(geometry);
}

void SnappedScrollBar::Private::updateProxyScrollBarSize()
{
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

bool SnappedScrollBar::Private::eventFilter(QObject* object, QEvent* event)
{
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

// ------------------------------------------------------------------------------------------------
// SnappedScrollBar

SnappedScrollBar::SnappedScrollBar(QWidget* parent):
    SnappedScrollBar(Qt::Vertical, parent)
{
}

SnappedScrollBar::SnappedScrollBar(Qt::Orientation orientation, QWidget* parent):
    base_type(orientation, parent),
    d(new Private(this))
{
}

SnappedScrollBar::~SnappedScrollBar()
{
}

Qt::Alignment SnappedScrollBar::alignment() const
{
    return d->alignment;
}

void SnappedScrollBar::setAlignment(Qt::Alignment alignment)
{
    if (d->alignment == alignment)
        return;

    d->alignment = alignment;
    updateGeometry();
}

bool SnappedScrollBar::useItemViewPaddingWhenVisible() const
{
    return d->useItemViewPaddingWhenVisible;
}

void SnappedScrollBar::setUseItemViewPaddingWhenVisible(bool useItemViewPaddingWhenVisible)
{
    if (d->useItemViewPaddingWhenVisible == useItemViewPaddingWhenVisible)
        return;

    d->useItemViewPaddingWhenVisible = useItemViewPaddingWhenVisible;
    d->updateProxyScrollBarSize();
}

bool SnappedScrollBar::useMaximumSpace() const
{
    return d->useMaximumSpace;
}

void SnappedScrollBar::setUseMaximumSpace(bool useMaximumSpace)
{
    if (d->useMaximumSpace == useMaximumSpace)
        return;

    d->useMaximumSpace = useMaximumSpace;
    d->updateGeometry();
}

ScrollBarProxy* SnappedScrollBar::proxyScrollBar() const
{
    return d->proxyScrollbar;
}

} // namespace nx::vms::client::desktop
