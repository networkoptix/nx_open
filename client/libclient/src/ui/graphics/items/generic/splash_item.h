#ifndef QN_SPLASH_ITEM_H
#define QN_SPLASH_ITEM_H

#include <QtGui/QBrush>
#include <QtWidgets/QGraphicsObject>

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>

class QnSplashItem: public Animated<QGraphicsObject>, public AnimationTimerListener {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(SplashType splashType READ splashType WRITE setSplashType)
    Q_ENUMS(SplashType)
    typedef Animated<QGraphicsObject> base_type;

public:
    enum SplashType {
        Circular,
        Rectangular,
        Invalid = -1
    };

    QnSplashItem(QGraphicsItem *parent = NULL);
    virtual ~QnSplashItem();

    SplashType splashType() const {
        return m_splashType;
    }

    void setSplashType(SplashType splashType) {
        NX_ASSERT(splashType == Circular || splashType == Rectangular || splashType == Invalid);

        m_splashType = splashType;
    }

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color);

    virtual QRectF boundingRect() const override {
        return rect();
    }

    const QRectF &rect() const {
        return m_rect;
    }

    void setRect(const QRectF &rect);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void animate(qint64 endTimeMSec, const QRectF &endRect, qreal endOpacity, bool destroy = false, qint64 midTimeMSec = -1, qreal midOpacity = 1.0);

signals:
    void animationFinished();

protected:
    virtual void tick(int deltaMSecs) override;

private:
    struct AnimationData {
        qint64 time, midTime, endTime;
        qreal startOpacity, midOpacity, endOpacity;
        QRectF startRect, endRect;
        bool destroy;
    };

    /** Splash type. */
    SplashType m_splashType;

    /** Splash color. */
    QColor m_color;

    /** Brushes that are used for painting. 0-3 for rectangular splash, 4 for circular. */
    QBrush m_brushes[5];

    /** Bounding rectangle of the splash. */
    QRectF m_rect;

    /** Whether this splash item is animated. */
    QScopedPointer<AnimationData> m_animation;
};


#endif // QN_SPLASH_ITEM_H
