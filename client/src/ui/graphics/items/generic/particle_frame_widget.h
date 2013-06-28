#ifndef QN_PARTICLE_FRAME_WIDGET_H
#define QN_PARTICLE_FRAME_WIDGET_H

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnParticleItem;

class QnParticleFrameWidget: public Animated<GraphicsWidget>, public AnimationTimerListener {
    Q_OBJECT;
    typedef Animated<GraphicsWidget> base_type;

public:
    QnParticleFrameWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnParticleFrameWidget();

protected:
    virtual void tick(int deltaMSecs) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

private:
    void advanceParticlePosition(const QRectF &rect, qreal dt, Particle *particle) const;
    void updateItemPosition(const QRectF &rect, Particle *particle) const;

private:
    struct Particle {
        Particle(): item(NULL), relativePos(0.0), absoluteSpeed(0.0), relativeSpeed(0.0) {}

        QnParticleItem *item;
        qreal relativePos;
        qreal absoluteSpeed;
        qreal relativeSpeed;
    };

    QList<Particle> m_particles;
};


#endif // QN_PARTICLE_FRAME_WIDGET_H
