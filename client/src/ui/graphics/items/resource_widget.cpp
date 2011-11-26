#include "resource_widget.h"
#include <cassert>
#include <QPainter>
#include <QGraphicsLinearLayout>
#include <core/resource/resource_media_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <camera/resource_display.h>
#include <utils/common/warnings.h>
#include <utils/common/qt_opengl.h>
#include <settings.h>
#include "resource_widget_renderer.h"
#include "polygonal_shadow_item.h"

namespace {
    /** Default frame width. */
    const qreal defaultFrameWidth = 1.0;

    /** Default frame color. */
    const QColor defaultFrameColor = QColor(128, 128, 128, 196);

    const QColor selectedFrameColorMixIn = QColor(255, 255, 255, 0);

    /** Frame extension multiplier determines the width of frame extension relative
     * to frame width.
     * 
     * Frame events are processed not only when hovering over the frame itself, 
     * but also over its extension. */
    const qreal frameExtensionMultiplier = 1.0;

    /** Default shadow displacement, in scene coordinates. */
    const QPointF defaultShadowDisplacement = QPointF(5.0, 5.0);

    /** Default timeout before the video is displayed as "loading", in milliseconds. */
    const qint64 defaultLoadingTimeoutMSec = 2000;

    /** Default period of progress circle. */
    const qint64 defaultProgressPeriodMSec = 1000;

    /** Default progress painter. */
    Q_GLOBAL_STATIC_WITH_ARGS(QnLoadingProgressPainter, progressPainter, (0.5, 12, 0.5, QColor(255, 255, 255, 0), QColor(255, 255, 255, 255)));
}

QnResourceWidget::QnResourceWidget(QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(parent),
    m_item(item),
    m_videoLayout(NULL),
    m_channelCount(0),
    m_renderer(NULL),
    m_aspectRatio(-1.0),
    m_enclosingAspectRatio(1.0),
    m_frameWidth(0.0),
    m_aboutToBeDestroyedEmitted(false),
    m_lastNewFrameTimeMSec(QDateTime::currentMSecsSinceEpoch())
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

    /* Set up buttons layout. */
    m_buttonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    m_buttonsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_buttonsLayout->insertStretch(0, 0x1000); /* Set large enough stretch for the item to be placed in right end of the layout. */
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    layout->addItem(m_buttonsLayout);
    layout->addStretch(0x1000);
    setLayout(layout);

    /* Set up video rendering. */
    m_display = item->createDisplay(this);
    m_videoLayout = m_display->videoLayout();
    m_channelCount = m_videoLayout->numberOfChannels();
    
    m_renderer = new QnResourceWidgetRenderer(m_channelCount);
    connect(m_renderer, SIGNAL(sourceSizeChanged(const QSize &)), this, SLOT(at_sourceSizeChanged(const QSize &)));
    m_display->addRenderer(m_renderer);

    m_display->start();
}

QnResourceWidget::~QnResourceWidget() {
    ensureAboutToBeDestroyedEmitted();

    delete m_display;
    
    if(!m_shadow.isNull()) {
        m_shadow.data()->setShapeProvider(NULL);
        delete m_shadow.data();
    }
}

const QnResourcePtr &QnResourceWidget::resource() const {
    return m_display->resource();
}

void QnResourceWidget::setFrameWidth(qreal frameWidth) {
    prepareGeometryChange();
    
    m_frameWidth = frameWidth;
    qreal extendedFrameWidth = m_frameWidth * (1.0 + frameExtensionMultiplier);
    setWindowFrameMargins(extendedFrameWidth, extendedFrameWidth, extendedFrameWidth, extendedFrameWidth);

    invalidateShadowShape();
    if(m_shadow.data() != NULL)
        m_shadow.data()->setSoftWidth(m_frameWidth);
}

void QnResourceWidget::setShadowDisplacement(const QPointF &displacement) {
    m_shadowDisplacement = displacement;

    updateShadowPos();
}

void QnResourceWidget::setEnclosingAspectRatio(qreal enclosingAspectRatio) {
    m_enclosingAspectRatio = enclosingAspectRatio;
}

QRectF QnResourceWidget::enclosingGeometry() const {
    return expanded(m_enclosingAspectRatio, geometry(), Qt::KeepAspectRatioByExpanding);
}

void QnResourceWidget::setEnclosingGeometry(const QRectF &enclosingGeometry) {
    m_enclosingAspectRatio = enclosingGeometry.width() / enclosingGeometry.height();

    if(hasAspectRatio()) {
        setGeometry(expanded(m_aspectRatio, enclosingGeometry, Qt::KeepAspectRatio));
    } else {
        setGeometry(enclosingGeometry);
    }
}

QPolygonF QnResourceWidget::provideShape() {
    QTransform transform = sceneTransform();
    QPointF zero = transform.map(QPointF());
    transform = transform * QTransform::fromTranslate(-zero.x(), -zero.y());

    qreal fw2 = m_frameWidth / 2;
    return transform.map(QPolygonF(QRectF(QPointF(0, 0), size()).adjusted(-fw2, -fw2, fw2, fw2)));
}

void QnResourceWidget::invalidateShadowShape() {
    if(m_shadow.isNull())
        return;

    m_shadow.data()->invalidateShape();
}

void QnResourceWidget::updateShadowZ() {
    if(!m_shadow.isNull()) {
        m_shadow.data()->setZValue(zValue());
        m_shadow.data()->stackBefore(this);
    }
}

void QnResourceWidget::updateShadowPos() {
    if(!m_shadow.isNull())
        m_shadow.data()->setPos(mapToScene(0.0, 0.0) + m_shadowDisplacement);
}

void QnResourceWidget::setGeometry(const QRectF &geometry) {
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

    base_type::setGeometry(geometry);
    setTransformOriginPoint(rect().center());
}

QSizeF QnResourceWidget::constrainedSize(const QSizeF constraint) const {
    if(!hasAspectRatio())
        return constraint;

    return expanded(m_aspectRatio, constraint, Qt::KeepAspectRatio);
}

QSizeF QnResourceWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    QSizeF result = base_type::sizeHint(which, constraint);

    if(!hasAspectRatio())
        return result;

    if(which == Qt::MinimumSize)
        return expanded(m_aspectRatio, result, Qt::KeepAspectRatioByExpanding);

    return result;
}

void QnResourceWidget::at_sourceSizeChanged(const QSize &size) {
    qreal oldAspectRatio = m_aspectRatio;
    qreal newAspectRatio = static_cast<qreal>(size.width() * m_videoLayout->width()) / (size.height() * m_videoLayout->height());
    if(qFuzzyCompare(oldAspectRatio, newAspectRatio))
        return;

    QRectF enclosingGeometry = this->enclosingGeometry();
    m_aspectRatio = newAspectRatio;

    updateGeometry(); /* Discard cached size hints. */
    setGeometry(expanded(m_aspectRatio, enclosingGeometry, Qt::KeepAspectRatio));

    emit aspectRatioChanged(oldAspectRatio, newAspectRatio);
}

QVariant QnResourceWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
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

void QnResourceWidget::resizeEvent(QGraphicsSceneResizeEvent *event) override {
    invalidateShadowShape();

    base_type::resizeEvent(event);
}

QRectF QnResourceWidget::channelRect(int channel) const {
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

void QnResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/) {
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

void QnResourceWidget::drawLoadingProgress(QnRenderStatus::RenderStatus status, const QRectF &rect) {
    qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();

    if(status == QnRenderStatus::RENDERED_NEW_FRAME) {
        m_lastNewFrameTimeMSec = currentTimeMSec;
        return;
    }
        
    if(status == QnRenderStatus::RENDERED_OLD_FRAME && currentTimeMSec - m_lastNewFrameTimeMSec < defaultLoadingTimeoutMSec)
        return;

    glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    if(status == QnRenderStatus::RENDERED_OLD_FRAME) {
        glColor4f(0.0, 0.0, 0.0, 0.5);
    } else {
        glColor4f(0.0, 0.0, 0.0, 1.0);
    }
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
    progressPainter()->paint(static_cast<qreal>(currentTimeMSec % defaultProgressPeriodMSec) / defaultProgressPeriodMSec);
    glPopMatrix();

    glPopAttrib();
}

void QnResourceWidget::paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
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

Qt::WindowFrameSection QnResourceWidget::windowFrameSectionAt(const QPointF &pos) const override {
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

bool QnResourceWidget::windowFrameEvent(QEvent *event) {
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

void QnResourceWidget::addButton(QGraphicsLayoutItem *button) {
    m_buttonsLayout->insertItem(1, button);
}

void QnResourceWidget::removeButton(QGraphicsLayoutItem *button) {
    m_buttonsLayout->removeItem(button);
}

void QnResourceWidget::ensureAboutToBeDestroyedEmitted() {
    if(m_aboutToBeDestroyedEmitted)
        return;

    m_aboutToBeDestroyedEmitted = true;
    emit aboutToBeDestroyed();
}
