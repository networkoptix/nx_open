#include "uielementsinstrument.h"
#include <QGraphicsWidget>

UiElementsInstrument::UiElementsInstrument(QObject *parent):
    Instrument(Instrument::VIEWPORT, makeSet(QEvent::Resize), parent) 
{}

UiElementsInstrument::~UiElementsInstrument() {
    ensureUninstalled();
}

void UiElementsInstrument::installedNotify() {
    QGraphicsWidget *widget = new QGraphicsWidget();
    widget->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    scene()->addItem(widget);

    m_widget = widget;
}

void UiElementsInstrument::uninstalledNotify() {
    if(!m_widget.isNull()) {
        delete m_widget.data();
        m_widget.clear();
    }
}

bool UiElementsInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    QGraphicsWidget *widget = m_widget.data();
    if(widget == NULL)
        return false;

    QGraphicsView *view = this->view(viewport);

    QRectF newGeometry = QRectF(view->mapToScene(0, 0), viewport->size());
    if(!qFuzzyCompare(newGeometry, widget->geometry()))
        widget->setGeometry(newGeometry);

    return false;
}
