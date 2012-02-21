#include "ui_elements_instrument.h"
#include <QGraphicsWidget>
#include "destruction_guard_item.h"

UiElementsInstrument::UiElementsInstrument(QObject *parent):
    Instrument(Instrument::VIEWPORT, makeSet(QEvent::Paint), parent) 
{
    /* It is important NOT to use ItemIgnoresTransformations flag here as it 
     * messes up graphics scene framework's internals in a really awful way.
     * 
     * Possible problems:
     * QGraphicsItem::isUnderMouse returning wrong result.
     * QGraphicsItem::mapToScene returning wrong result.
     * I'm sure there are more. */
    QGraphicsWidget *widget = new QGraphicsWidget();
    widget->setParent(this);
    widget->setAcceptedMouseButtons(0);
    m_widget = widget;

    connect(widget, SIGNAL(zChanged()), this, SLOT(at_widget_zValueChanged()));
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
    
    at_widget_zValueChanged();
}

void UiElementsInstrument::uninstalledNotify() {
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

void UiElementsInstrument::at_widget_zValueChanged() {
    if(guard() == NULL || widget() == NULL) 
        return;

    guard()->setZValue(widget()->zValue());
}