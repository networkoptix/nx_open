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

    int particleCount() const;
    void setParticleCount(int particleCount);
    
    QColor particleColor() const;
    void setParticleColor(const QColor &particleColor);

    QSizeF particleSize() const;
    void setParticleSize(const QSizeF &particleSize);

    void regenerateParticles();

    void setParticleSpeed(qreal absoluteSpeed, qreal absoluteSpeedDeviation, qreal relativeSpeed, qreal relativeSpeedDeviation);
    void getParticleSpeed(qreal *absoluteSpeed, qreal *absoluteSpeedDeviation, qreal *relativeSpeed, qreal *relativeSpeedDeviation) const;

protected:
    virtual void tick(int deltaMSecs) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void updateListening();

private:
    struct Particle {
        Particle(): item(NULL), relativePos(0.0), absoluteSpeed(0.0), relativeSpeed(0.0) {}

        QnParticleItem *item;
        qreal relativePos;
        qreal absoluteSpeed;
        qreal relativeSpeed;
    };

    void regenerateParticle(Particle *particle) const;
    void advanceParticlePosition(const QRectF &rect, qreal dt, Particle *particle) const;
    void updateItemPosition(const QRectF &rect, Particle *particle) const;

private:
    QList<Particle> m_particles;
    qreal m_absoluteSpeed, m_absoluteSpeedDeviation, m_relativeSpeed, m_relativeSpeedDeviation;
    QColor m_color;
    QSizeF m_size;
};


#endif // QN_PARTICLE_FRAME_WIDGET_H
