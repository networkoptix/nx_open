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

        default:
            break;
    }

    return false;
}

void Instrument::registerQmlType()
{
    qmlRegisterType<Instrument>("Nx.Client.Desktop.Ui.Scene", 1, 0, "Instrument");
    qmlRegisterUncreatableType<MouseEvent>("Nx.Client.Desktop.Ui.Scene", 1, 0, "MouseEvent",
        lit("Cannot create instance of MouseEvent"));
}

} // namespace scene
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
