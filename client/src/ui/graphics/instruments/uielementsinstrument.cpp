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

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout();
    widget->setLayout(layout);

    
    QLabel *label = new QLabel("123412341234234");
    QGraphicsProxyWidget *ww = new QGraphicsProxyWidget();
    ww->setWidget(label);

    layout->addItem(ww);
    layout->setAlignment(ww, Qt::AlignCenter);

    m_widget = widget;
}

void UiElementsInstrument::uninstalledNotify() {
    if(!m_widget.isNull()) {
        delete m_widget.data();
        m_widget.clear();
    }
}

void UiElementsInstrument::adjustPosition(QGraphicsView *view) {
    QGraphicsWidget *widget = m_widget.data();
    if(widget == NULL)
        return;

    widget->resize(view->viewport()->size());
    widget->setPos(view->mapToScene(0, 0));
}
