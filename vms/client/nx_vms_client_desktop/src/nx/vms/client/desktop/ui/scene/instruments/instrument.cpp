// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "instrument.h"

#include <QtGui/QMouseEvent>
#include <QtQml/QtQml>
#include <QtQuick/QQuickItem>

#include "instrument_events.h"

namespace nx::vms::client::desktop {
namespace ui {
namespace scene {

class Instrument::Private
{
    Instrument* const q = nullptr;

public:
    Private(Instrument* parent);

    QPointer<QObject> item;
    bool enabled = true;

    bool hoverEnabled = false;
    Qt::MouseButtons acceptedButtons = Qt::LeftButton;

    bool handleMouseButtonPress(QMouseEvent* event);
    bool handleMouseButtonRelease(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseButtonDblClick(QMouseEvent* event);
    bool handleMouseWheel(QWheelEvent* event);
    bool handleHoverEnter(QHoverEvent* event);
    bool handleHoverLeave(QHoverEvent* event);
    bool handleHoverMove(QHoverEvent* event);
    bool handleKeyPress(QKeyEvent* event);
    bool handleKeyRelease(QKeyEvent* event);

    void updateHover();
    void updateButtons();
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

bool Instrument::Private::handleMouseWheel(QWheelEvent* event)
{
    WheelEvent wheel(event);
    emit q->mouseWheel(&wheel);
    return wheel.accepted;
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

bool Instrument::Private::handleKeyPress(QKeyEvent* event)
{
    KeyEvent key(event);
    emit q->keyPress(&key);
    return key.accepted;
}

bool Instrument::Private::handleKeyRelease(QKeyEvent* event)
{
    KeyEvent key(event);
    emit q->keyRelease(&key);
    return key.accepted;
}

//-------------------------------------------------------------------------------------------------

Instrument::~Instrument()
{
    setEnabled(false);
}

QObject* Instrument::item() const
{
    return d->item;
}

void Instrument::setItem(QObject* value)
{
    if (d->item == value)
        return;

    if (d->item && d->enabled)
        d->item->removeEventFilter(this);

    d->item = value;

    if (d->enabled && value)
        value->installEventFilter(this);

    d->updateHover();
    d->updateButtons();

    emit itemChanged();
}

bool Instrument::enabled() const
{
    return d->enabled;
}

void Instrument::setEnabled(bool value)
{
    if (d->enabled == value)
        return;

    d->enabled = value;
    emit enabledChanged();

    if (!d->item)
        return;

    if (value)
        d->item->installEventFilter(this);
    else
        d->item->removeEventFilter(this);
}


bool Instrument::hoverEnabled() const
{
    return d->hoverEnabled;
}

void Instrument::setHoverEnabled(bool value)
{
    if (d->hoverEnabled == value)
        return;

    d->hoverEnabled = value;
    d->updateHover();

    emit hoverEnabledChanged();
}

Qt::MouseButtons Instrument::acceptedButtons() const
{
    return d->acceptedButtons;
}

void Instrument::setAcceptedButtons(Qt::MouseButtons value)
{
    if (d->acceptedButtons == value)
        return;

    d->acceptedButtons = value;
    d->updateButtons();

    emit acceptedButtonsChanged();
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
        case QEvent::Wheel:
            return d->handleMouseWheel(static_cast<QWheelEvent*>(event));
        case QEvent::HoverEnter:
            return d->handleHoverEnter(static_cast<QHoverEvent*>(event));
        case QEvent::HoverLeave:
            return d->handleHoverLeave(static_cast<QHoverEvent*>(event));
        case QEvent::HoverMove:
            return d->handleHoverMove(static_cast<QHoverEvent*>(event));
        case QEvent::KeyPress:
            return d->handleKeyPress(static_cast<QKeyEvent*>(event));
        case QEvent::KeyRelease:
            return d->handleKeyRelease(static_cast<QKeyEvent*>(event));

        default:
            break;
    }

    return false;
}

void Instrument::Private::updateHover()
{
    if (auto quickItem = qobject_cast<QQuickItem*>(item))
        quickItem->setAcceptHoverEvents(hoverEnabled);
}

void Instrument::Private::updateButtons()
{
    if (auto quickItem = qobject_cast<QQuickItem*>(item))
        quickItem->setAcceptedMouseButtons(acceptedButtons);
}

void Instrument::registerQmlType()
{
    qmlRegisterType<Instrument>("nx.vms.client.desktop", 1, 0, "Instrument");
    qmlRegisterUncreatableType<MouseEvent>("nx.vms.client.desktop", 1, 0, "MouseEvent",
        "Cannot create instance of MouseEvent");
    qmlRegisterUncreatableType<HoverEvent>("nx.vms.client.desktop", 1, 0, "HoverEvent",
        "Cannot create instance of HoverEvent");
    qmlRegisterUncreatableType<WheelEvent>("nx.vms.client.desktop", 1, 0, "WheelEvent",
        "Cannot create instance of WheelEvent");
    qmlRegisterUncreatableType<KeyEvent>("nx.vms.client.desktop", 1, 0, "KeyEvent",
        "Cannot create instance of KeyEvent");
}

} // namespace scene
} // namespace ui
} // namespace nx::vms::client::desktop
