#include "splash_item.h"

#include <QtGui/QGradient>
#include <QtGui/QLinearGradient>
#include <QtGui/QRadialGradient>
#include <QtGui/QPainter>

#include <utils/common/delete_later.h>
#include <utils/math/color_transformations.h>
#include <nx/utils/math/fuzzy.h>
#include <utils/math/linear_combination.h>

QnSplashItem::QnSplashItem(QGraphicsItem *parent):
    base_type(parent),
    m_splashType(Invalid)
{
    setAcceptedMouseButtons(0);
    setColor(Qt::white);
}

QnSplashItem::~QnSplashItem() {
    return;
}

void QnSplashItem::setColor(const QColor &color) {
    if(m_color == color)
        return;

    m_color = color;

    /* Sector numbering for rectangular splash:
     *       1
     *      0 2
     *       3                                                              */
    QGradient gradients[5];
    gradients[0] = QLinearGradient(1.0, 0.0, 0.0, 0.0);
    gradients[1] = QLinearGradient(0.0, 1.0, 0.0, 0.0);
    gradients[2] = QLinearGradient(0.0, 0.0, 1.0, 0.0);
    gradients[3] = QLinearGradient(0.0, 0.0, 0.0, 1.0);
    gradients[4] = QRadialGradient(0.5, 0.5, 0.5);

    for(int i = 0; i < 5; i++) {
        gradients[i].setCoordinateMode(QGradient::ObjectBoundingMode);
        gradients[i].setColorAt(0.8, toTransparent(m_color));
        gradients[i].setColorAt(0.9, m_color);
        gradients[i].setColorAt(1.0, toTransparent(m_color));
        m_brushes[i] = QBrush(gradients[i]);
    }
}

void QnSplashItem::setRect(const QRectF &rect) {
    if(qFuzzyEquals(m_rect, rect))
        return;

    prepareGeometryChange();

    m_rect = rect;
}

void QnSplashItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(m_splashType == Circular) {
        painter->fillRect(m_rect, m_brushes[4]);
    } else if(splashType() == Rectangular) {
        QPointF points[5] = {
            m_rect.bottomLeft(),
            m_rect.topLeft(),
            m_rect.topRight(),
            m_rect.bottomRight(),
            m_rect.bottomLeft()
        };
        
        qreal d = qMin(m_rect.width(), m_rect.height()) / 2;
        QPointF centers[5] = {
            m_rect.bottomLeft()     + QPointF( d, -d),
            m_rect.topLeft()        + QPointF( d,  d),
            m_rect.topRight()       + QPointF(-d,  d),
            m_rect.bottomRight()    + QPointF(-d, -d),
            m_rect.bottomLeft()     + QPointF( d, -d)
        };

        for(int i = 0; i < 4; i++) {
            QPainterPath path;

            path = QPainterPath();
            path.moveTo(centers[i]);
            path.lineTo(points[i]);
            path.lineTo(points[i + 1]);
            path.lineTo(centers[i + 1]);
            path.closeSubpath();
            painter->fillPath(path, m_brushes[i]);
        }
    }
}
        
void QnSplashItem::animate(qint64 endTimeMSec, const QRectF &endRect, qreal endOpacity, bool destroy, qint64 midTimeMSec, qreal midOpacity) {
    if(!m_animation) {
        m_animation.reset(new AnimationData());
        registerAnimation(this);
    }
        
    startListening();

    AnimationData *a = m_animation.data();
    a->time = 0;
    a->midTime = midTimeMSec;
    a->endTime = endTimeMSec;
    a->startOpacity = opacity();
    a->midOpacity = midOpacity;
    a->endOpacity = endOpacity;
    a->startRect = rect();
    a->endRect = endRect;
    a->destroy = destroy;
}

void QnSplashItem::tick(int deltaMSecs) {
    AnimationData *a = m_animation.data();
    a->time = qMin(a->time + deltaMSecs, a->endTime);

    qreal progress = static_cast<qreal>(a->time) / a->endTime;
    setRect(linearCombine(1.0 - progress, a->startRect, progress, a->endRect));

    if(a->midTime < 0) {
        setOpacity(linearCombine(1.0 - progress, a->startOpacity, progress, a->endOpacity));
    } else {
        if(a->time < a->midTime) {
            qreal progress = static_cast<qreal>(a->time) / a->midTime;
            setOpacity(linearCombine(1.0 - progress, a->startOpacity, progress, a->midOpacity));
        } else {
            qreal progress = static_cast<qreal>(a->time - a->midTime) / (a->endTime - a->midTime);
            setOpacity(linearCombine(1.0 - progress, a->midOpacity, progress, a->endOpacity));
        }
    }

    if(a->time >= a->endTime) {
        stopListening();

        emit animationFinished();

        if(a->destroy)
            qnDeleteLater(this);
    }
}

