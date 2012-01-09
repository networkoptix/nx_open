#ifndef QN_OPACITY_HOVER_ITEM_H
#define QN_OPACITY_HOVER_ITEM_H

#include <ui/processors/hover_processor.h>

class VariantAnimator;
class AnimationTimer;

class QnOpacityHoverItem: public HoverProcessor {
    Q_OBJECT;
    Q_PROPERTY(qreal targetOpacity READ targetOpacity WRITE setTargetOpacity);

    typedef HoverProcessor base_type;

public:
    QnOpacityHoverItem(AnimationTimer *animationTimer, QGraphicsItem *parent = NULL);

    qreal targetOpacity() const;

    void setTargetOpacity(qreal opacity);

    void setTargetHoverOpacity(qreal opacity);

    void setTargetNormalOpacity(qreal opacity);

    void setAnimationSpeed(qreal speed);

    void setAnimationTimeLimit(qreal timeLimitMSec);

    virtual QRectF boundingRect() const override {
        return QRectF();
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

protected:
    void updateTargetOpacity(bool animate);

protected slots:
    void at_hoverEntered();
    void at_hoverLeft();

private:
    VariantAnimator *m_animator;
    qreal m_normalOpacity;
    qreal m_hoverOpacity;
    bool m_underMouse;
};

#endif // QN_OPACITY_HOVER_ITEM_H

