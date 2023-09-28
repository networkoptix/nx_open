// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_DESTRUCTION_GUARD_ITEM_H
#define QN_DESTRUCTION_GUARD_ITEM_H

#include <QtWidgets/QGraphicsObject>
#include <QtWidgets/QGraphicsScene>

#include <ui/common/weak_graphics_item_pointer.h>

class DestructionGuardItem: public QGraphicsObject {
    Q_OBJECT;
public:
    DestructionGuardItem(QGraphicsItem *parent = nullptr):
        QGraphicsObject(parent)
    {
        setFlags(ItemHasNoContents);
        setAcceptedMouseButtons(Qt::NoButton);
        setPos(0.0, 0.0);
    }

    virtual ~DestructionGuardItem() {
        /* Don't let it be deleted. */
        if(guarded() != nullptr && guarded()->scene() != nullptr)
            guarded()->scene()->removeItem(guarded());
    }

    void setGuarded(QGraphicsItem *guarded) {
        if(!m_guarded.isNull() && guarded->toGraphicsObject())
            disconnect(guarded->toGraphicsObject(), nullptr, this, nullptr);

        m_guarded = guarded;

        if(guarded != nullptr) {
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
