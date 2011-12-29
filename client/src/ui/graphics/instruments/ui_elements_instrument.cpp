#include "ui_elements_instrument.h"
#include <QGraphicsWidget>

class DestructionGuardItem: public QGraphicsObject {
public:
    DestructionGuardItem(QGraphicsItem *parent = NULL): 
        QGraphicsObject(parent) 
    {}

    virtual ~DestructionGuardItem() {
        /* Don't let it be deleted. */
        if(guarded() != NULL && guarded()->scene() != NULL)
            guarded()->scene()->removeItem(guarded());
    }

    void setGuarded(QGraphicsObject *guarded) {
        m_guarded = guarded;

        if(guarded != NULL)
            guarded->setParentItem(this);
    }

    QGraphicsObject *guarded() const {
        return m_guarded.data();
    }

    virtual QRectF boundingRect() const override {
        return QRectF();
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

private:
    QWeakPointer<QGraphicsObject> m_guarded;
};


UiElementsInstrument::UiElementsInstrument(QObject *parent):
    Instrument(Instrument::VIEWPORT, makeSet(QEvent::Paint), parent) 
{
    QGraphicsWidget *widget = new QGraphicsWidget();
    widget->setParent(this);
    widget->setFlag(QGraphicsItem::ItemIgnoresTransformations);
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

    QGraphicsView *view = this->view(viewport);

    QRectF newGeometry = QRectF(view->mapToScene(0, 0), viewport->size());
    if(!qFuzzyCompare(newGeometry, widget()->geometry()))
        widget()->setGeometry(newGeometry);

    return false;
}

void UiElementsInstrument::at_widget_zValueChanged() {
    if(guard() == NULL || widget() == NULL) 
        return;

    guard()->setZValue(widget()->zValue());
}