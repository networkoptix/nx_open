#ifndef QN_DESTRUCTION_GUARD_ITEM_H
#define QN_DESTRUCTION_GUARD_ITEM_H

#include <QGraphicsObject>

class DestructionGuardItem: public QGraphicsObject {
    Q_OBJECT;
public:
    DestructionGuardItem(QGraphicsItem *parent = NULL): 
        QGraphicsObject(parent) 
    {
        setFlags(ItemHasNoContents);
        setAcceptedMouseButtons(0);
    }

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

#endif // QN_DESTRUCTION_GUARD_ITEM_H
