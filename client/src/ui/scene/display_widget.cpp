#include "display_widget.h"
#include <cassert>
#include <QPainter>
#include <utils/common/warnings.h>
#include <utils/common/qt_opengl.h>
#include <core/resource/resource_media_layout.h>
#include <core/dataprovider/media_streamdataprovider.h>
#include <camera/gl_renderer.h>
#include <camera/camdisplay.h>
#include <ui/model/ui_layout_item.h>
#include <settings.h>
#include "display_widget_renderer.h"
#include "polygonal_shadow_item.h"

namespace {
    /** Default frame width. */
    qreal defaultFrameWidth = 1.0;

    /** Default frame color. */
    QColor defaultFrameColor = QColor(128, 128, 128, 196);

    /** Frame extension multiplier determines the width of frame extension relative
     * to frame width.
     * 
     * Frame events are processed not only when hovering over the frame itself, 
     * but also over its extension. */
    qreal frameExtensionMultiplier = 1.0;

    /** Default shadow displacement, in scene coordinates. */
    QPointF defaultShadowDisplacement = QPointF(5.0, 5.0);
}

QnDisplayWidget::QnDisplayWidget(QnUiLayoutItem *entity, QGraphicsItem *parent):
    base_type(parent),
    m_entity(entity),
    m_resourceLayout(NULL),
    m_channelCount(0),
    m_renderer(NULL),
    m_frameWidth(0.0)
{
    /* Set up video rendering. */
    m_resourceLayout = entity->mediaResource()->getVideoLayout(entity->mediaProvider());
    m_channelCount = m_resourceLayout->numberOfChannels();
    m_renderer = new QnDisplayWidgetRenderer(m_channelCount, this);
    for(int i = 0; i < m_channelCount; i++)
        m_entity->camDisplay()->addVideoChannel(i, m_renderer, true);

    /* Set up shadow. */
    m_shadow = QWeakPointer<QnPolygonalShadowItem>(new QnPolygonalShadowItem());
    QnPolygonalShadowItem *shadow = m_shadow.data();
    shadow->setParent(this);
    shadow->setColor(global_shadow_color);
    shadow->setShapeProvider(this);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setShadowDisplacement(defaultShadowDisplacement);
    invalidateShadowShape();

    /* Set up frame. */
    setFrameColor(defaultFrameColor);
    setFrameWidth(defaultFrameWidth);
}

QnDisplayWidget::~QnDisplayWidget() {
    m_renderer->beforeDestroy();
    
    if(!m_shadow.isNull()) 
        m_shadow.data()->setShapeProvider(NULL);
}

void QnDisplayWidget::setFrameWidth(qreal frameWidth) {
    prepareGeometryChange();
    
    m_frameWidth = frameWidth;
    qreal extendedFrameWidth = m_frameWidth * (1.0 + frameExtensionMultiplier);
    setWindowFrameMargins(extendedFrameWidth, extendedFrameWidth, extendedFrameWidth, extendedFrameWidth);

    invalidateShadowShape();
    if(m_shadow.data() != NULL)
        m_shadow.data()->setSoftWidth(m_frameWidth);
}

void QnDisplayWidget::setShadowDisplacement(const QPointF &displacement) {
    m_shadowDisplacement = displacement;

    updateShadowPos();
}

QPolygonF QnDisplayWidget::provideShape() {
    QTransform transform;
    QPointF zero = mapToScene(0.0, 0.0);
    transform.translate(-zero.x(), -zero.y());
    transform *= sceneTransform();

    qreal fw2 = m_frameWidth / 2;
    return transform.map(QPolygonF(QRectF(QPointF(0, 0), size()).adjusted(-fw2, -fw2, fw2, fw2)));
}

void QnDisplayWidget::invalidateShadowShape() {
    if(m_shadow.isNull())
        return;

    m_shadow.data()->invalidateShape();
}

void QnDisplayWidget::updateShadowZ() {
    if(!m_shadow.isNull()) {
        m_shadow.data()->setZValue(zValue());
        m_shadow.data()->stackBefore(this);
    }
}

void QnDisplayWidget::updateShadowPos() {
    if(!m_shadow.isNull())
        m_shadow.data()->setPos(mapToScene(0.0, 0.0) + m_shadowDisplacement);
}

QVariant QnDisplayWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    switch(change) {
    case ItemPositionHasChanged:
        updateShadowPos();
        break;
    case ItemTransformHasChanged:
    case ItemRotationHasChanged:
    case ItemScaleHasChanged:
    case ItemTransformOriginPointHasChanged: 
        invalidateShadowShape();
        break;
    case ItemSceneHasChanged:
        if(scene() != NULL && !m_shadow.isNull()) {
            scene()->addItem(m_shadow.data());
            updateShadowZ();
            updateShadowPos();
        }
        break;
    case ItemZValueHasChanged:
        updateShadowZ();
        break;
    default:
        break;
    }

    return base_type::itemChange(change, value);
}

void QnDisplayWidget::resizeEvent(QGraphicsSceneResizeEvent *event) override {
    invalidateShadowShape();

    base_type::resizeEvent(event);
}

QRectF QnDisplayWidget::channelRect(int channel) const {
    if (m_channelCount == 1) 
        return QRectF(QPointF(0.0, 0.0), size());

    QSizeF size = this->size();
    qreal w = size.width() / m_resourceLayout->width();
    qreal h = size.height() / m_resourceLayout->height();

    return QRectF(
        w * m_resourceLayout->v_position(channel), 
        h * m_resourceLayout->h_position(channel), 
        w, 
        h
    );
}

void QnDisplayWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/) {
    if (painter->paintEngine()->type() != QPaintEngine::OpenGL && painter->paintEngine()->type() != QPaintEngine::OpenGL2) {
        qnWarning("Painting with the paint engine of type %1 is not supported", static_cast<int>(painter->paintEngine()->type()));
        return;
    }

    /* Update screen size of a single channel. */
    QSizeF itemScreenSize = painter->combinedTransform().mapRect(boundingRect()).size();
    QSize channelScreenSize = QSizeF(itemScreenSize.width() / m_resourceLayout->width(), itemScreenSize.height() / m_resourceLayout->height()).toSize();
    if(channelScreenSize != m_channelScreenSize) {
        m_channelScreenSize = channelScreenSize;
        m_renderer->setChannelScreenSize(m_channelScreenSize);
    }
    
    /* Draw content. */
    painter->beginNativePainting();
    for(int i = 0; i < m_channelCount; i++) {
        CLGLRenderer *renderer = m_renderer->channelRenderer(i);

        if(!renderer->paintEvent(channelRect(i))) {
          /* TODO: Failure */
        }
    }
    painter->endNativePainting();
}

void QnDisplayWidget::paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
    QSizeF size = this->size();
    qreal w = size.width();
    qreal h = size.height();
    qreal fw = m_frameWidth;

    /* Note that we use OpenGL drawing functions here for a reason.
     * If drawing with QPainter, off-by-one pixel errors may occur as video is drawn via OpenGL. */
    painter->beginNativePainting();
    glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
    glColor(m_frameColor);
    glBegin(GL_QUADS);
    glVertices(QRectF(-fw,     -fw,     w + fw * 2,  fw));
    glVertices(QRectF(-fw,     h,       w + fw * 2,  fw));
    glVertices(QRectF(-fw,     0,       fw,          h));
    glVertices(QRectF(w,       0,       fw,          h));
    glEnd();
    glPopAttrib();
    painter->endNativePainting();
}

namespace {
    inline Qt::WindowFrameSection sweep(qreal v, qreal v0, qreal v1, qreal v2, qreal v3, Qt::WindowFrameSection fs1, Qt::WindowFrameSection fs2, Qt::WindowFrameSection fs3) {
        if(v < v1) {
            if(v < v0) {
                return Qt::NoSection;
            } else {
                return fs1;
            }
        } else {
            if(v < v2) {
                return fs2;
            } else if(v < v3) {
                return fs3;
            } else {
                return Qt::NoSection;
            }
        }
    }

}

Qt::WindowFrameSection QnDisplayWidget::windowFrameSectionAt(const QPointF &pos) const override {
    QSizeF size = this->size();

    /* Shortcuts for position. */
    qreal x = pos.x();
    qreal y = pos.y();

    /* Calculate extended frame width. */
    qreal fe = m_frameWidth * frameExtensionMultiplier;
    qreal fw = m_frameWidth;

    /* Calculate border values. */
    qreal x0 = -fw - fe;
    qreal x1 = fe;
    qreal x2 = size.width() - fe;
    qreal x3 = size.width() + fw + fe;
    qreal y0 = -fw -fe;
    qreal y1 = fe;
    qreal y2 = size.height() - fe;
    qreal y3 = size.height() + fw + fe;

    /* Sweep. */
    if(x < x1) {
        if(x < x0) {
            return Qt::NoSection;
        } else {
            return sweep(y, y0, y1, y2, y3, Qt::TopLeftSection, Qt::LeftSection, Qt::BottomLeftSection);
        }
    } else {
        if(x < x2) {
            return sweep(y, y0, y1, y2, y3, Qt::TopSection, Qt::NoSection, Qt::BottomSection);
        } else if(x < x3) {
            return sweep(y, y0, y1, y2, y3, Qt::TopRightSection, Qt::RightSection, Qt::BottomRightSection);
        } else {
            return Qt::NoSection;
        }
    }
}

