#include "scroll_bar_proxy.h"

#include <QtCore/QEvent>
#include <QtWidgets/QAbstractScrollArea>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

ScrollBarProxy::ScrollBarProxy(QWidget* parent):
    base_type(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumSize(1, 1);
}

ScrollBarProxy::ScrollBarProxy(QScrollBar* scrollBar, QWidget* parent):
    ScrollBarProxy(parent)
{
    setScrollBar(scrollBar);
}

ScrollBarProxy::~ScrollBarProxy()
{
}

void ScrollBarProxy::setScrollBar(QScrollBar* scrollBar)
{
    if (m_scrollBar == scrollBar)
        return;

    if (m_scrollBar)
        m_scrollBar->disconnect(this);

    m_scrollBar = scrollBar;

    if (m_scrollBar)
    {
        m_scrollBar->setRange(minimum(), maximum());
        m_scrollBar->setPageStep(pageStep());
        m_scrollBar->setSingleStep(singleStep());
        m_scrollBar->setValue(value());
        m_scrollBar->setVisible(m_visible);

        setOrientation(m_scrollBar->orientation());

        connect(m_scrollBar, &ScrollBarProxy::valueChanged, this, &QScrollBar::setValue);
    }
}

QSize ScrollBarProxy::sizeHint() const
{
    return QSize(qMax(minimumWidth(), 1), qMax(minimumHeight(), 1));
}

bool ScrollBarProxy::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::Hide:
            setScrollBarVisible(false);
            break;

        case QEvent::Show:
            setScrollBarVisible(true);
            break;

        case QEvent::Paint:
            break;

        case QEvent::ParentAboutToChange:
            if (parentWidget())
                parentWidget()->removeEventFilter(this);
            break;

        case QEvent::ParentChange:
            if (!parentWidget())
                break;
            parentWidget()->installEventFilter(this);
            setScrollBarVisible(isVisible() && parentWidget()->isVisible());
            break;

        default:
            break;
    }

    return base_type::event(event);
}

void ScrollBarProxy::makeProxy(QScrollBar* scrollBar, QAbstractScrollArea* scrollArea)
{
    NX_ASSERT(scrollBar);

    ScrollBarProxy* proxy = new ScrollBarProxy(scrollBar, scrollArea);

    if (scrollBar->orientation() == Qt::Vertical)
        scrollArea->setVerticalScrollBar(proxy);
    else
        scrollArea->setHorizontalScrollBar(proxy);
}

void ScrollBarProxy::paintEvent(QPaintEvent* /*event*/)
{
}

void ScrollBarProxy::sliderChange(SliderChange change)
{
    base_type::sliderChange(change);
    if (!m_scrollBar)
        return;

    switch (change)
    {
        case SliderStepsChange:
            m_scrollBar->setPageStep(pageStep());
            m_scrollBar->setSingleStep(singleStep());
            break;

        case SliderValueChange:
            m_scrollBar->setValue(value());
            break;

        case SliderRangeChange:
            m_scrollBar->setRange(minimum(), maximum());
            break;

        default:
            break;
    }
}

void ScrollBarProxy::setScrollBarVisible(bool visible)
{
    if (m_visible == visible)
        return;

    m_visible = visible;

    if (m_scrollBar)
        m_scrollBar->setVisible(m_visible);
}

} // namespace nx::vms::client::desktop
