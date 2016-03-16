#include "scroll_bar_proxy.h"

#include <QtCore/QEvent>
#include <QtWidgets/QAbstractScrollArea>

QnScrollBarProxy::QnScrollBarProxy(QWidget *parent)
    : QScrollBar(parent)
    , m_scrollBar(nullptr)
    , m_visible(true)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumSize(1, 1);
}

QnScrollBarProxy::QnScrollBarProxy(QScrollBar *scrollBar, QWidget *parent)
    : QScrollBar(parent)
    , m_scrollBar(nullptr)
    , m_visible(true)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setScrollBar(scrollBar);
}

QnScrollBarProxy::~QnScrollBarProxy()
{
}

void QnScrollBarProxy::setScrollBar(QScrollBar *scrollBar)
{
    if (m_scrollBar == scrollBar)
        return;

    if (m_scrollBar)
    {
        disconnect(m_scrollBar, nullptr, this, nullptr);
        disconnect(this, nullptr, m_scrollBar, nullptr);
    }

    m_scrollBar = scrollBar;

    if (m_scrollBar)
    {
        m_scrollBar->setRange(minimum(), maximum());
        m_scrollBar->setValue(value());
        m_scrollBar->setVisible(m_visible);
        setOrientation(m_scrollBar->orientation());

        connect(this, &QnScrollBarProxy::rangeChanged, m_scrollBar, &QScrollBar::setRange);
        connect(this, &QnScrollBarProxy::valueChanged, m_scrollBar, &QScrollBar::setValue);
        connect(m_scrollBar, &QnScrollBarProxy::valueChanged, this, &QScrollBar::setValue);
    }
}

QSize QnScrollBarProxy::sizeHint() const
{
    return QSize(qMax(minimumWidth(), 1), qMax(minimumHeight(), 1));
}

bool QnScrollBarProxy::event(QEvent *event)
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
        if (parentWidget())
        {
            parentWidget()->installEventFilter(this);
            setScrollBarVisible(isVisible() && parentWidget()->isVisible());
        }
        break;
    default:
        break;
    }

    return base_type::event(event);
}

void QnScrollBarProxy::makeProxy(QScrollBar *scrollBar, QAbstractScrollArea *scrollArea)
{
    NX_ASSERT(scrollBar);

    QnScrollBarProxy *proxy = new QnScrollBarProxy(scrollBar, scrollArea);

    if (scrollBar->orientation() == Qt::Vertical)
        scrollArea->setVerticalScrollBar(proxy);
    else
        scrollArea->setHorizontalScrollBar(proxy);
}

void QnScrollBarProxy::paintEvent(QPaintEvent *)
{
}

void QnScrollBarProxy::setScrollBarVisible(bool visible)
{
    if (m_visible == visible)
        return;

    m_visible = visible;

    if (m_scrollBar)
        m_scrollBar->setVisible(m_visible);
}
