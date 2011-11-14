#include "display_widget.h"
#include <cassert>
#include <QPainter>
#include <utils/common/warnings.h>
#include <utils/common/qt_opengl.h>
#include <utils/common/scene_utility.h>
#include <core/resource/resource_media_layout.h>
#include <core/dataprovider/media_streamdataprovider.h>
#include <camera/gl_renderer.h>
#include <camera/camdisplay.h>
#include <ui/model/resource_item_model.h>
#include <ui/control/resource_display.h>
#include <settings.h>
#include "display_widget_renderer.h"
#include "polygonal_shadow_item.h"

namespace {
    /** Default frame width. */
    qreal defaultFrameWidth = 1.0;

    /** Default frame color. */
    QColor defaultFrameColor = QColor(128, 128, 128, 196);

    QColor selectedFrameColorMixIn = QColor(255, 255, 255, 0);

    /** Frame extension multiplier determines the width of frame extension relative
     * to frame width.
     * 
     * Frame events are processed not only when hovering over the frame itself, 
     * but also over its extension. */
    qreal frameExtensionMultiplier = 1.0;

    /** Default shadow displacement, in scene coordinates. */
    QPointF defaultShadowDisplacement = QPointF(5.0, 5.0);
}

QnDisplayWidget::QnDisplayWidget(QnLayoutItemModel *item, QGraphicsItem *parent):
    base_type(parent),
    m_item(item),
    m_resourceLayout(NULL),
    m_channelCount(0),
    m_renderer(NULL),
    m_aspectRatio(0.0),
    m_frameWidth(0.0)
{
    /* Set up shadow. */
    m_shadow = new QnPolygonalShadowItem();
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

    /* Set up video rendering. */
    m_display = item->createDisplay(this);
    m_resourceLayout = m_display->mediaResource()->getVideoLayout(m_display->mediaProvider());
    m_channelCount = m_resourceLayout->numberOfChannels();
    m_renderer = new QnDisplayWidgetRenderer(m_channelCount, this);
    connect(m_renderer, SIGNAL(sourceSizeChanged(const QSize &)), this, SLOT(at_sourceSizeChanged(const QSize &)));
    for(int i = 0; i < m_channelCount; i++)
        m_display->camDisplay()->addVideoChannel(i, m_renderer, true);

    m_display->start();
}

QnDisplayWidget::~QnDisplayWidget() {
    m_renderer->beforeDestroy();
    
    if(!m_shadow.isNull()) {
        m_shadow.data()->setShapeProvider(NULL);
        delete m_shadow.data();
    }
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

void QnDisplayWidget::setGeometry(const QRectF &geometry) {
    /* Unfortunately, widgets with constant aspect ratio cannot be implemented
     * using size hints. So here is one of the workarounds. */
    
    if(qFuzzyIsNull(m_aspectRatio)) {
        base_type::setGeometry(geometry);
        return;
    }

    qreal aspectRatio = geometry.width() / geometry.height();
    if(qFuzzyCompare(m_aspectRatio, aspectRatio)) {
        base_type::setGeometry(geometry);
        return;
    }

    /* Calculate actual new size. */
    QSizeF newSize = constrainedSize(geometry.size());

    /* Find anchor point and calculate new position. */
    QRectF oldGeometry = this->geometry();

    qreal newLeft;
    if(qFuzzyCompare(oldGeometry.right(), geometry.right())) {
        newLeft = oldGeometry.right() - newSize.width();
    } else {
        newLeft = geometry.left();
    }

    qreal newTop;
    if(qFuzzyCompare(oldGeometry.bottom(), geometry.bottom())) {
        newTop = oldGeometry.bottom() - newSize.height();
    } else {
        newTop = geometry.top();
    }

    base_type::setGeometry(QRectF(QPointF(newLeft, newTop), newSize));
}

QSizeF QnDisplayWidget::constrainedSize(const QSizeF constraint) const {
    if(qFuzzyIsNull(m_aspectRatio))
        return constraint;

    return QnSceneUtility::expanded(m_aspectRatio, constraint, Qt::KeepAspectRatio);
}

void QnDisplayWidget::at_sourceSizeChanged(const QSize &size) {
    qreal oldAspectRatio = m_aspectRatio;
    qreal newAspectRatio = static_cast<qreal>(size.width() * m_resourceLayout->width()) / (size.height() * m_resourceLayout->height());
    if(qFuzzyCompare(oldAspectRatio, newAspectRatio))
        return;

    m_aspectRatio = newAspectRatio;
    setGeometry(geometry());
    emit aspectRatioChanged(oldAspectRatio, newAspectRatio);
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

    /* Prepare color. */
    QColor color = m_frameColor;
    if(isSelected()) {
        color.setRed  ((color.red()   + selectedFrameColorMixIn.red())   / 2);
        color.setGreen((color.green() + selectedFrameColorMixIn.green()) / 2);
        color.setBlue ((color.blue()  + selectedFrameColorMixIn.blue())  / 2);
    }

    /* Note that we use OpenGL drawing functions here for a reason.
     * If drawing with QPainter, off-by-one pixel errors may occur as video is drawn via OpenGL. */
    painter->beginNativePainting();
    glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
    glColor(color);
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
    inline Qt::WindowFrameSection sweep(qreal v, qreal v0, qreal v1, Qt::WindowFrameSection fs0, Qt::WindowFrameSection fs1, Qt::WindowFrameSection fs2) {
        if(v < v0) {
            return fs0;
        } else {
            if(v < v1) {
                return fs1;
            } else {
                return fs2;
            }
        }
    }

    inline Qt::WindowFrameSection sweep(const QRectF &geometry, const QPointF &pos) {
        /* Shortcuts for position. */
        qreal x = pos.x();
        qreal y = pos.y();

        /* Border shortcuts. */
        qreal x0 = geometry.left();
        qreal x1 = geometry.right();
        qreal y0 = geometry.top();
        qreal y1 = geometry.bottom();

        /* Sweep. */
        if(x < x0) {
            return sweep(y, y0, y1, Qt::TopLeftSection, Qt::LeftSection, Qt::BottomLeftSection);
        } else {
            if(x < x1) {
                return sweep(y, y0, y1, Qt::TopSection, Qt::NoSection, Qt::BottomSection);
            } else {
                return sweep(y, y0, y1, Qt::TopRightSection, Qt::RightSection, Qt::BottomRightSection);
            }
        }
    }
}

Qt::WindowFrameSection QnDisplayWidget::windowFrameSectionAt(const QPointF &pos) const override {
    qreal fe = m_frameWidth * frameExtensionMultiplier;
    Qt::WindowFrameSection result = sweep(rect().adjusted(fe, fe, -fe, -fe), pos);
    
    /* This widget does not have side frame sections in case aspect ratio is set. */
    if(!qFuzzyIsNull(m_aspectRatio)) {
        switch(result) {
        case Qt::LeftSection:
        case Qt::RightSection:
        case Qt::TopSection:
        case Qt::BottomSection:
            result = Qt::NoSection;
            break;
        default:
            break;
        }
    }

    return result;
}

bool QnDisplayWidget::windowFrameEvent(QEvent *event) {
    bool result = base_type::windowFrameEvent(event);

    if(event->type() == QEvent::GraphicsSceneHoverMove) {
        QGraphicsSceneHoverEvent *e = static_cast<QGraphicsSceneHoverEvent *>(event);

        /* Qt does not unset a cursor unless mouse pointer leaves widget's frame. 
         * 
         * As this widget may not have a frame section associated with some parts of
         * its frame, cursor must be unset manually. */
        Qt::WindowFrameSection section = windowFrameSectionAt(e->pos());
        if(section == Qt::NoSection)
            unsetCursor();
    }

    return result;
}
