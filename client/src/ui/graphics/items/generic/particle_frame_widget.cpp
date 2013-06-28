#include "particle_frame_widget.h"

#include <utils/common/util.h>

#include <ui/common/geometry.h>
#include <ui/math/math.h>

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
    base_type(parent, windowFlags)
{
    registerAnimation(this);
    startListening();

    for(int i = 0; i < 10; i++) {
        Particle particle;
        particle.item = new QnParticleItem(this);
        particle.item->setRect(QRectF(-10, -10, 20, 20));
        particle.relativeSpeed = 1.0;
        particle.absoluteSpeed = 0.0;
        particle.relativePos = random() * 4.0;

        m_particles.push_back(particle);
    }
}

QnParticleFrameWidget::~QnParticleFrameWidget() {
    return;
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

    for(QList<Particle>::iterator pos = m_particles.begin(); pos != m_particles.end(); pos++)
        updateItemPosition(rect, &*pos);
}