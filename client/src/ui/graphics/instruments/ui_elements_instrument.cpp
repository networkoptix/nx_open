#include "ui_elements_instrument.h"

#include <ui/graphics/items/standard/graphics_widget.h>

#include "destruction_guard_item.h"

UiElementsInstrument::UiElementsInstrument(QObject *parent):
    Instrument(Instrument::Viewport, makeSet(QEvent::Paint), parent) 
{
    /* It is important NOT to use ItemIgnoresTransformations flag here as it 
     * messes up graphics scene framework's internals in a really awful way.
     * 
     * Possible problems:
     * QGraphicsItem::isUnderMouse returning wrong result.
     * QGraphicsItem::mapToScene returning wrong result.
     * I'm sure there are more. */
    QGraphicsWidget *widget = new GraphicsWidget();
    widget->setParent(this);
    widget->setAcceptedMouseButtons(0);
    m_widget = widget;
}

UiElementsInstrument::~UiElementsInstrument() {
    ensureUninstalled();
}

void UiElementsInstrument::installedNotify() {
    DestructionGuardItem *guard = new DestructionGuardItem();
    guard->setGuarded(widget());
    guard->setPos(0.0, 0.0);
    scene()->addItem(guard);

    m_guard = guard;
}

void UiElementsInstrument::aboutToBeUninstalledNotify() {
    if(guard() != NULL)
        delete guard();
}

bool UiElementsInstrument::paintEvent(QWidget *viewport, QPaintEvent *) {
    if(widget() == NULL)
        return false;

    QSizeF newSize = viewport->size();
    if(!qFuzzyCompare(newSize, widget()->size()))
        widget()->resize(newSize);

    QGraphicsView *view = this->view(viewport);
    widget()->setTransform(view->viewportTransform().inverted());

    return false;
}

