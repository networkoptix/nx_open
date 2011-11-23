#include "uielementsinstrument.h"
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

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
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
    m_widget = widget;
}

UiElementsInstrument::~UiElementsInstrument() {
    ensureUninstalled();
}

void UiElementsInstrument::installedNotify() {
    DestructionGuardItem *guard = new DestructionGuardItem();
    guard->setGuarded(m_widget.data());
    guard->setPos(0.0, 0.0);
    scene()->addItem(guard);

    m_guard = guard;
}

void UiElementsInstrument::uninstalledNotify() {
    if(!m_guard.isNull())
        delete m_guard.data();
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
