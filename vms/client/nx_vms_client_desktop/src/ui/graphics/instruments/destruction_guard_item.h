#ifndef QN_DESTRUCTION_GUARD_ITEM_H
#define QN_DESTRUCTION_GUARD_ITEM_H

#include <QtWidgets/QGraphicsObject>
#include <QtWidgets/QGraphicsScene>
#include <ui/common/weak_graphics_item_pointer.h>


class DestructionGuardItem: public QGraphicsObject {
    Q_OBJECT;
public:
    DestructionGuardItem(QGraphicsItem *parent = NULL): 
        QGraphicsObject(parent) 
    {
        setFlags(ItemHasNoContents);
        setAcceptedMouseButtons(0);
        setPos(0.0, 0.0);
    }

    virtual ~DestructionGuardItem() {
        /* Don't let it be deleted. */
        if(guarded() != NULL && guarded()->scene() != NULL)
            guarded()->scene()->removeItem(guarded());
    }

    void setGuarded(QGraphicsItem *guarded) {
        if(!m_guarded.isNull() && guarded->toGraphicsObject())
            disconnect(guarded->toGraphicsObject(), NULL, this, NULL);

        m_guarded = guarded;

        if(guarded != NULL) {
            guarded->setParentItem(this);

            if(guarded->toGraphicsObject())
                connect(guarded->toGraphicsObject(), &QGraphicsObject::zChanged, this, &DestructionGuardItem::at_guarded_zValueChanged);
        }
    }

    QGraphicsItem *guarded() const {
        return m_guarded.data();
    }

    virtual QRectF boundingRect() const override {
        return QRectF();
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

protected slots:
    void at_guarded_zValueChanged() {
        setZValue(guarded()->zValue());
    }

private:
    WeakGraphicsItemPointer m_guarded;
};

#endif // QN_DESTRUCTION_GUARD_ITEM_H
