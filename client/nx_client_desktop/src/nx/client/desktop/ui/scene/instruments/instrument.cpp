#include "instrument.h"

#include <QtGui/QMouseEvent>
#include <QtQuick/QQuickItem>
#include <QtQml/QtQml>

#include "instrument_events.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace scene {

class Instrument::Private
{
    Instrument* const q = nullptr;

public:
    Private(Instrument* parent);

    QPointer<QQuickItem> item;
    bool enabled = true;

    bool handleMouseButtonPress(QMouseEvent* event);
    bool handleMouseButtonRelease(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseButtonDblClick(QMouseEvent* event);
    bool handleHoverEnter(QHoverEvent* event);
    bool handleHoverLeave(QHoverEvent* event);
    bool handleHoverMove(QHoverEvent* event);
};

Instrument::Instrument(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

Instrument::Private::Private(Instrument* parent):
    q(parent)
{
}

bool Instrument::Private::handleMouseButtonPress(QMouseEvent* event)
{
    MouseEvent mouse(event);
    emit q->mousePress(&mouse);
    return mouse.accepted;
}

bool Instrument::Private::handleMouseButtonRelease(QMouseEvent* event)
{
    MouseEvent mouse(event);
    emit q->mouseRelease(&mouse);
    return mouse.accepted;
}

bool Instrument::Private::handleMouseMove(QMouseEvent* event)
{
    MouseEvent mouse(event);
    emit q->mouseMove(&mouse);
    return mouse.accepted;

}

bool Instrument::Private::handleMouseButtonDblClick(QMouseEvent* event)
{
    MouseEvent mouse(event);
    emit q->mouseDblClick(&mouse);
    return mouse.accepted;
}

bool Instrument::Private::handleHoverEnter(QHoverEvent* event)
{
    HoverEvent hover(event);
    emit q->hoverEnter(&hover);
    return hover.accepted;
}

bool Instrument::Private::handleHoverLeave(QHoverEvent* event)
{
    HoverEvent hover(event);
    emit q->hoverLeave(&hover);
    return hover.accepted;
}

bool Instrument::Private::handleHoverMove(QHoverEvent* event)
{
    HoverEvent hover(event);
    emit q->hoverMove(&hover);
    return hover.accepted;
}

//-------------------------------------------------------------------------------------------------

Instrument::~Instrument()
{
    setEnabled(false);
}

QQuickItem* Instrument::item() const
{
    return d->item;
}

void Instrument::setItem(QQuickItem* item)
{
    if (d->item == item)
        return;

    if (d->item && d->enabled)
        d->item->removeEventFilter(this);

    d->item = item;

    if (d->enabled)
        item->installEventFilter(this);

    emit itemChanged();
}

bool Instrument::enabled() const
{
    return d->enabled;
}

void Instrument::setEnabled(bool enabled)
{
    if (d->enabled == enabled)
        return;

    d->enabled = enabled;
    emit enabledChanged();

    if (!d->item)
        return;

    if (enabled)
        d->item->installEventFilter(this);
    else
        d->item->removeEventFilter(this);
}

bool Instrument::eventFilter(QObject* object, QEvent* event)
{
    if (object != d->item)
        return false;

    switch (event->type())
    {
        case QEvent::MouseButtonPress:
            return d->handleMouseButtonPress(static_cast<QMouseEvent*>(event));
        case QEvent::MouseButtonRelease:
            return d->handleMouseButtonRelease(static_cast<QMouseEvent*>(event));
        case QEvent::MouseMove:
            return d->handleMouseMove(static_cast<QMouseEvent*>(event));
        case QEvent::MouseButtonDblClick:
            return d->handleMouseButtonDblClick(static_cast<QMouseEvent*>(event));
        case QEvent::HoverEnter:
            return d->handleHoverEnter(static_cast<QHoverEvent*>(event));
        case QEvent::HoverLeave:
            return d->handleHoverLeave(static_cast<QHoverEvent*>(event));
        case QEvent::HoverMove:
            return d->handleHoverMove(static_cast<QHoverEvent*>(event));

        default:
            break;
    }

    return false;
}

void Instrument::registerQmlType()
{
    qmlRegisterType<Instrument>("nx.client.desktop", 1, 0, "Instrument");
    qmlRegisterUncreatableType<MouseEvent>("nx.client.desktop", 1, 0, "MouseEvent",
        lit("Cannot create instance of MouseEvent"));
    qmlRegisterUncreatableType<HoverEvent>("nx.client.desktop", 1, 0, "HoverEvent",
        lit("Cannot create instance of HoverEvent"));
}

} // namespace scene
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
