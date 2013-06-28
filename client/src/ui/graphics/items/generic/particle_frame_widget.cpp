#include "particle_frame_widget.h"

#include <utils/common/util.h>
#include <utils/math/math.h>

#include <ui/common/geometry.h>

#include "particle_item.h"

namespace {
    qreal toRelativePos(const QRectF &rect, qreal absolutePos) {
        /*****************
         *     Sides:    *
         *               *
         *    +--1--+    *
         *    |     |    *
         *    4     2    *
         *    |     |    *
         *    +--3--+    *
         *               *
         *****************/
        qreal half = rect.width() + rect.height();

        if(absolutePos < half) {
            if(absolutePos < rect.width()) {
                return absolutePos / rect.width();
            } else {
                return 1.0 + (absolutePos - rect.width()) / rect.height();
            }
        } else {
            if(absolutePos < half + rect.width()) {
                return 2.0 + (absolutePos - half) / rect.width();
            } else {
                return 3.0 + (absolutePos - half - rect.width()) / rect.height();
            }
        }
    }

    qreal toAbsolutePos(const QRectF &rect, qreal relativePos) {
        if(relativePos < 2.0) {
            if(relativePos < 1.0) {
                return rect.width() * relativePos;
            } else {
                return rect.width() + rect.height() * (relativePos - 1.0);
            }
        } else {
            if(relativePos < 3.0) {
                return rect.height() + rect.width() * (relativePos - 1.0);
            } else {
                return rect.width() * 2.0 + rect.height() * (relativePos - 2.0);
            }
        }
    }

    QPointF toItemPos(const QRectF &rect, qreal relativePos) {
        if(relativePos < 2.0) {
            if(relativePos < 1.0) {
                return rect.topLeft() + QPointF(rect.width() * relativePos, 0.0);
            } else {
                return rect.topRight() + QPointF(0.0, rect.height() * (relativePos - 1.0));
            }
        } else {
            if(relativePos < 3.0) {
                return rect.bottomRight() - QPointF(rect.width() * (relativePos - 2.0), 0.0);
            } else {
                return rect.bottomLeft() - QPointF(0.0, rect.height() * (relativePos - 3.0));
            }
        }
    }

} // anonymous namespace

QnParticleFrameWidget::QnParticleFrameWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_absoluteSpeed(0.0),
    m_absoluteSpeedDeviation(0.0),
    m_relativeSpeed(1.0),
    m_relativeSpeedDeviation(1.0),
    m_color(Qt::black),
    m_size(1, 1)
{
    registerAnimation(this);
}

QnParticleFrameWidget::~QnParticleFrameWidget() {
    return;
}

int QnParticleFrameWidget::particleCount() const {
    return m_particles.size();
}

void QnParticleFrameWidget::setParticleCount(int particleCount) {
    if(m_particles.size() == particleCount)
        return;

    while(particleCount < m_particles.size())
        m_particles.pop_back();

    while(particleCount > m_particles.size()) {
        Particle particle;
        particle.item = new QnParticleItem(this);
        particle.relativePos = random() * 4.0;

        regenerateParticle(&particle);

        m_particles.push_back(particle);
    }

    updateListening();
}

QColor QnParticleFrameWidget::particleColor() const {
    return m_color;
}

void QnParticleFrameWidget::setParticleColor(const QColor &particleColor) {
    m_color = particleColor;
}

QSizeF QnParticleFrameWidget::particleSize() const {
    return m_size;
}

void QnParticleFrameWidget::setParticleSize(const QSizeF &particleSize) {
    m_size = particleSize;
}

void QnParticleFrameWidget::regenerateParticles() {
    for(QList<Particle>::iterator pos = m_particles.begin(); pos != m_particles.end(); pos++)
        regenerateParticle(&*pos);
}

void QnParticleFrameWidget::setParticleSpeed(qreal absoluteSpeed, qreal absoluteSpeedDeviation, qreal relativeSpeed, qreal relativeSpeedDeviation) {
    m_absoluteSpeed = absoluteSpeed;
    m_absoluteSpeedDeviation = absoluteSpeedDeviation;
    m_relativeSpeed = relativeSpeed;
    m_relativeSpeedDeviation = relativeSpeedDeviation;
}

void QnParticleFrameWidget::getParticleSpeed(qreal *absoluteSpeed, qreal *absoluteSpeedDeviation, qreal *relativeSpeed, qreal *relativeSpeedDeviation) const {
    if(absoluteSpeed)
        *absoluteSpeed = m_absoluteSpeed;
    if(absoluteSpeedDeviation)
        *absoluteSpeedDeviation = m_absoluteSpeedDeviation;
    if(relativeSpeed)
        *relativeSpeed = m_relativeSpeed;
    if(relativeSpeedDeviation)
        *relativeSpeedDeviation = m_relativeSpeedDeviation;
}

void QnParticleFrameWidget::regenerateParticle(Particle *particle) const {
    particle->absoluteSpeed = m_absoluteSpeed + m_absoluteSpeedDeviation * (random() * 2.0 - 1.0);
    particle->relativeSpeed = m_relativeSpeed + m_relativeSpeedDeviation * (random() * 2.0 - 1.0);
    particle->item->setColor(m_color);
    particle->item->setRect(QRectF(-QnGeometry::toPoint(m_size) / 2.0, m_size));

    if(random(0, 2) == 0) {
        particle->absoluteSpeed = -particle->absoluteSpeed;
        particle->relativeSpeed = -particle->relativeSpeed;
    }
}

void QnParticleFrameWidget::advanceParticlePosition(const QRectF &rect, qreal dt, Particle *particle) const {
    qreal speed = particle->absoluteSpeed + particle->relativeSpeed * QnGeometry::length(rect.size());

    particle->relativePos = toRelativePos(
        rect, 
        qMod(toAbsolutePos(rect, particle->relativePos) + speed * dt, (rect.width() + rect.height()) * 2)
    );
}

void QnParticleFrameWidget::updateItemPosition(const QRectF &rect, Particle *particle) const {
    particle->item->setPos(toItemPos(rect, particle->relativePos));
}

void QnParticleFrameWidget::updateListening() {
    if(particleCount() < 0 || !isVisible() || qFuzzyIsNull(opacity())) {
        stopListening();
    } else {
        startListening();
    }
}

void QnParticleFrameWidget::tick(int deltaMSecs) {
    qreal dt = deltaMSecs / 1000.0;
    QRectF rect = this->rect();

    for(QList<Particle>::iterator pos = m_particles.begin(); pos != m_particles.end(); pos++) {
        advanceParticlePosition(rect, dt, &*pos);
        updateItemPosition(rect, &*pos);
    }
}

void QnParticleFrameWidget::resizeEvent(QGraphicsSceneResizeEvent *event) {
    base_type::resizeEvent(event);

    QRectF rect = this->rect();
    for(QList<Particle>::iterator pos = m_particles.begin(); pos != m_particles.end(); pos++)
        updateItemPosition(rect, &*pos);
}

QVariant QnParticleFrameWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    QVariant result = base_type::itemChange(change, value);

    if(change == ItemVisibleHasChanged || change == ItemOpacityHasChanged)
        updateListening();

    return result;
}

