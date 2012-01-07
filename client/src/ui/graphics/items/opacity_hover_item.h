#ifndef QN_OPACITY_HOVER_ITEM_H
#define QN_OPACITY_HOVER_ITEM_H

#include <QGraphicsObject>

class VariantAnimator;
class AnimationTimer;

class QnOpacityHoverItem: public QGraphicsObject {
    Q_OBJECT;
    Q_PROPERTY(qreal targetOpacity READ targetOpacity WRITE setTargetOpacity);

    typedef QGraphicsObject base_type;

public:
    QnOpacityHoverItem(AnimationTimer *animationTimer, QGraphicsItem *target);

    qreal targetOpacity() const;

    void setTargetOpacity(qreal opacity);

    void setTargetHoverOpacity(qreal opacity);

    void setTargetNormalOpacity(qreal opacity);

    void setAnimationSpeed(qreal speed);

    void setAnimationTimeLimit(qreal timeLimitMSec);

    bool isTargetUnderMouse() const {
        return m_underMouse;
    }

    virtual QRectF boundingRect() const override {
        return QRectF();
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override;

    void updateTargetOpacity(bool animate);

private:
    VariantAnimator *m_animator;
    qreal m_normalOpacity;
    qreal m_hoverOpacity;
    bool m_underMouse;
};

#endif // QN_OPACITY_HOVER_ITEM_H

