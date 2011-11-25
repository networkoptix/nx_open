#include "display_widget.h"
#include <cassert>
#include <QPainter>
#include <core/resource/resource_media_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <camera/resource_display.h>
#include <utils/common/warnings.h>
#include <utils/common/qt_opengl.h>
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

    Q_GLOBAL_STATIC_WITH_ARGS(QnLoadingProgressPainter, progressPainter, (0.5, 12, 0.5, QColor(255, 255, 255, 0), QColor(255, 255, 255, 255)));
}

QnDisplayWidget::QnDisplayWidget(QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(parent),
    m_item(item),
    m_videoLayout(NULL),
    m_channelCount(0),
    m_renderer(NULL),
    m_aspectRatio(-1.0),
    m_enclosingAspectRatio(1.0),
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
    m_videoLayout = m_display->videoLayout();
    m_channelCount = m_videoLayout->numberOfChannels();
    
    m_renderer = new QnDisplayWidgetRenderer(m_channelCount);
    connect(m_renderer, SIGNAL(sourceSizeChanged(const QSize &)), this, SLOT(at_sourceSizeChanged(const QSize &)));
    m_display->addRenderer(m_renderer);

    m_display->start();
}

QnDisplayWidget::~QnDisplayWidget() {
    delete m_display;
    
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

void QnDisplayWidget::setEnclosingAspectRatio(qreal enclosingAspectRatio) {
    m_enclosingAspectRatio = enclosingAspectRatio;
}

QRectF QnDisplayWidget::enclosingGeometry() const {
    return expanded(m_enclosingAspectRatio, geometry(), Qt::KeepAspectRatioByExpanding);
}

void QnDisplayWidget::setEnclosingGeometry(const QRectF &enclosingGeometry) {
    m_enclosingAspectRatio = enclosingGeometry.width() / enclosingGeometry.height();

    if(hasAspectRatio()) {
        setGeometry(expanded(m_aspectRatio, enclosingGeometry, Qt::KeepAspectRatio));
    } else {
        setGeometry(enclosingGeometry);
    }
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
    
#if 0
    if(!hasAspectRatio()) {
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
#endif

    return base_type::setGeometry(geometry);
}

QSizeF QnDisplayWidget::constrainedSize(const QSizeF constraint) const {
    if(!hasAspectRatio())
        return constraint;

    return expanded(m_aspectRatio, constraint, Qt::KeepAspectRatio);
}

void QnDisplayWidget::at_sourceSizeChanged(const QSize &size) {
    qreal oldAspectRatio = m_aspectRatio;
    qreal newAspectRatio = static_cast<qreal>(size.width() * m_videoLayout->width()) / (size.height() * m_videoLayout->height());
    if(qFuzzyCompare(oldAspectRatio, newAspectRatio))
        return;

    QRectF enclosingGeometry = this->enclosingGeometry();
    m_aspectRatio = newAspectRatio;
    setGeometry(expanded(m_aspectRatio, enclosingGeometry, Qt::KeepAspectRatio));

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
    qreal w = size.width() / m_videoLayout->width();
    qreal h = size.height() / m_videoLayout->height();

    return QRectF(
        w * m_videoLayout->h_position(channel), 
        h * m_videoLayout->v_position(channel), 
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
    QSize channelScreenSize = QSizeF(itemScreenSize.width() / m_videoLayout->width(), itemScreenSize.height() / m_videoLayout->height()).toSize();
    if(channelScreenSize != m_channelScreenSize) {
        m_channelScreenSize = channelScreenSize;
        m_renderer->setChannelScreenSize(m_channelScreenSize);
    }
    
    /* Draw content. */
    painter->beginNativePainting();
    for(int i = 0; i < m_channelCount; i++) {
        QRectF rect = channelRect(i);
        QnRenderStatus::RenderStatus status = m_renderer->paint(i, rect);
        drawLoadingProgress(status, rect);
    }
    painter->endNativePainting();
}

void QnDisplayWidget::drawLoadingProgress(QnRenderStatus::RenderStatus status, const QRectF &rect) const {
    if(status == QnRenderStatus::RENDERED_NEW_FRAME || status == QnRenderStatus::RENDERED_OLD_FRAME)
        return;

    glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    glColor3f(0.0, 0.0, 0.0);
    glBegin(GL_QUADS);
    glVertices(rect);
    glEnd();

    QRectF progressRect = expanded(
        1.0, 
        QRectF(
            rect.center() - toPoint(rect.size()) / 8,
            rect.size() / 4
        ),
        Qt::KeepAspectRatio
    );
    
    glPushMatrix();
    glTranslatef(progressRect.center().x(), progressRect.center().y(), 1.0);
    glScalef(progressRect.width() / 2, progressRect.height() / 2, 1.0);
    progressPainter()->paint(QDateTime::currentMSecsSinceEpoch() / 1000.0);
    glPopMatrix();

    glPopAttrib();
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
    
    /* This widget has no side frame sections in case aspect ratio is set. */
    if(hasAspectRatio()) {
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
