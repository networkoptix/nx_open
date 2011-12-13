#include "resource_widget.h"
#include <cassert>
#include <QPainter>
#include <QGraphicsLinearLayout>
#include <core/resource/resource_media_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <ui/graphics/painters/paused_painter.h>
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

    /** Default duration of "fade-in" effect for overlay icons. */
    const qint64 defaultOverlayFadeInDurationMSec = 500;

    /** Default progress painter. */
    Q_GLOBAL_STATIC_WITH_ARGS(QnLoadingProgressPainter, progressPainter, (0.5, 12, 0.5, QColor(255, 255, 255, 0), QColor(255, 255, 255, 255)));

    /** Default paused painter. */
    Q_GLOBAL_STATIC(QnPausedPainter, pausedPainter);
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
    m_activityDecorationsVisible(false),
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
        updateShadowPos();
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

void QnResourceWidget::showActivityDecorations() {
    m_activityDecorationsVisible = true;
}

void QnResourceWidget::hideActivityDecorations() {
    m_activityDecorationsVisible = false;
}

void QnResourceWidget::drawCurrentTime(QPainter *painter, const QRectF& rect, qint64 time)
{
    QString text = QDateTime::fromMSecsSinceEpoch(time/1000).toString("hh:mm:ss.zzz");
    if (!text.isEmpty())
    {
        QFont font;
        //font.setPixelSize(6);
        font.setPointSizeF(5.5);
        font.setStyleHint(QFont::SansSerif, QFont::ForceOutline);
        QFontMetrics metric(font);
        QSize size = metric.size(Qt::TextSingleLine, text);
        painter->setFont(font);
        painter->drawText(rect.width() - size.width()-4, rect.height() - size.height()+2, text);
    }
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
        
        /* Update time since the last new frame. */
        qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();
        if(status == QnRenderStatus::RENDERED_NEW_FRAME || m_display->isPaused())
            m_lastNewFrameTimeMSec = currentTimeMSec;

        /* Draw overlay icon if needed. */
        if(m_display->isPaused() && m_activityDecorationsVisible) {
            drawOverlayIcon(PAUSED, QColor(0, 0, 0, 128), 0.0, rect);
        } else if(status != QnRenderStatus::RENDERED_NEW_FRAME && (status != QnRenderStatus::RENDERED_OLD_FRAME || currentTimeMSec - m_lastNewFrameTimeMSec >= defaultLoadingTimeoutMSec)) {
            drawOverlayIcon(
                LOADING,
                QColor(0, 0, 0, status == QnRenderStatus::RENDERED_OLD_FRAME ? 128 : 255),
                static_cast<qreal>(currentTimeMSec % defaultProgressPeriodMSec) / defaultProgressPeriodMSec,
                rect
            );
        }
    }
    painter->endNativePainting();
    for(int i = 0; i < m_channelCount; i++) {
        qint64 time = m_renderer->lastDisplayedTime(i);
        if (time > 1000000ll * 3600*24)
            drawCurrentTime(painter, channelRect(i), time); // do not show time for regular media files
    }
}

void QnResourceWidget::drawOverlayIcon(OverlayIcon icon, QColor fillColor, qreal data, const QRectF &rect) {
    glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

    if(fillColor.alpha() != 0) {
        glColor(fillColor);
        glBegin(GL_QUADS);
        glVertices(rect);
        glEnd();
    }

    QRectF iconRect = expanded(
        1.0, 
        QRectF(
            rect.center() - toPoint(rect.size()) / 8,
            rect.size() / 4
        ),
        Qt::KeepAspectRatio
    );
    
    glPushMatrix();
    glTranslatef(iconRect.center().x(), iconRect.center().y(), 1.0);
    glScalef(iconRect.width() / 2, iconRect.height() / 2, 1.0);
    switch(icon) {
    case LOADING: 
        progressPainter()->paint(data);
        break;
    case PAUSED:
        pausedPainter()->paint(0.5);
        break;
    default:
        break;
    }
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

    painter->setRenderHint(QPainter::Antialiasing, true); /* Antialiasing is here for a reason. Without it border looks crappy. */
    painter->fillRect(QRectF(-fw,     -fw,     w + fw * 2,  fw), color);
    painter->fillRect(QRectF(-fw,     h,       w + fw * 2,  fw), color);
    painter->fillRect(QRectF(-fw,     0,       fw,          h),  color);
    painter->fillRect(QRectF(w,       0,       fw,          h),  color);
}

namespace {
    static const Qn::WindowFrameSection frameSectionTable[5][5] = {
        {Qn::NoSection, Qn::NoSection,          Qn::NoSection,      Qn::NoSection,          Qn::NoSection},
        {Qn::NoSection, Qn::TopLeftSection,     Qn::TopSection,     Qn::TopRightSection,    Qn::NoSection},
        {Qn::NoSection, Qn::LeftSection,        Qn::NoSection,      Qn::RightSection,       Qn::NoSection},
        {Qn::NoSection, Qn::BottomLeftSection,  Qn::BottomSection,  Qn::BottomRightSection, Qn::NoSection},
        {Qn::NoSection, Qn::NoSection,          Qn::NoSection,      Qn::NoSection,          Qn::NoSection}
    };

    inline int sweepIndex(qreal v, qreal v0, qreal v1, qreal v2, qreal v3) {
        if(v < v1) {
            if(v < v0) {
                return 0;
            } else {
                return 1;
            }
        } else {
            if(v < v2) {
                return 2;
            } else if(v < v3) {
                return 3;
            } else {
                return 4;
            }
        }
    }

    inline Qn::WindowFrameSections sweep(const QRectF &frameRect, const QRectF &rect, const QRectF &query) {
        /* Shortcuts for position. */
        qreal qx0 = query.left();
        qreal qx1 = query.right();
        qreal qy0 = query.top();
        qreal qy1 = query.bottom();

        /* Border shortcuts. */
        qreal x0 = frameRect.left();
        qreal x1 = rect.left();
        qreal x2 = rect.right();
        qreal x3 = frameRect.right();
        qreal y0 = frameRect.top();
        qreal y1 = rect.top();
        qreal y2 = rect.bottom();
        qreal y3 = frameRect.bottom();

        int cl = qMax(1, sweepIndex(qx0, x0, x1, x2, x3));
        int ch = qMin(3, sweepIndex(qx1, x0, x1, x2, x3));
        int rl = qMax(1, sweepIndex(qy0, y0, y1, y2, y3));
        int rh = qMin(3, sweepIndex(qy1, y0, y1, y2, y3));

        Qn::WindowFrameSections result = Qn::NoSection;
        for(int r = rl; r <= rh; r++)
            for(int c = cl; c <= ch; c++)
                result |= frameSectionTable[r][c];
        return result;
    }

} // anonymous namespace

Qt::WindowFrameSection QnResourceWidget::windowFrameSectionAt(const QPointF &pos) const override {
    return Qn::toQtFrameSection(static_cast<Qn::WindowFrameSection>(static_cast<int>(windowFrameSectionsAt(QRectF(pos, QSizeF(0.0, 0.0))))));
}

Qn::WindowFrameSections QnResourceWidget::windowFrameSectionsAt(const QRectF &region) const {
    Qn::WindowFrameSections result = sweep(windowFrameRect(), rect(), region);

    /* This widget has no side frame sections in case aspect ratio is set. */
    if(hasAspectRatio())
        result = result & ~(Qn::LeftSection | Qn::RightSection | Qn::TopSection | Qn::BottomSection);

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
