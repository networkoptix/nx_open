#include "resource_widget.h"
#include <cassert>
#include <QPainter>
#include <QGraphicsLinearLayout>
#include <core/resource/resource_media_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <ui/graphics/painters/paused_painter.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resourcemanagment/security_cam_resource.h>
#include <camera/resource_display.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <utils/common/warnings.h>
#include <utils/common/qt_opengl.h>
#include <utils/common/scoped_painter_rollback.h>

#include "polygonal_shadow_item.h"
#include "resource_widget_renderer.h"
#include "settings.h"

namespace {
    /** Default frame width. */
    const qreal defaultFrameWidth = 0.5;

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
    m_displayMotionGrid(false),
    m_motionMaskReady(false)
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
    m_resource = qnResPool->getResourceByUniqId(item->resourceUniqueId());
    m_display = new QnResourceDisplay(m_resource, this);
    connect(m_display, SIGNAL(resourceUpdated()), this, SLOT(at_display_resourceUpdated()));

    Q_ASSERT(m_display);
    m_videoLayout = m_display->videoLayout();
    Q_ASSERT(m_videoLayout);
    m_channelCount = m_videoLayout->numberOfChannels();

    m_renderer = new QnResourceWidgetRenderer(m_channelCount);
    connect(m_renderer, SIGNAL(sourceSizeChanged(const QSize &)), this, SLOT(at_sourceSizeChanged(const QSize &)));
    m_display->addRenderer(m_renderer);

    /* Set up overlay icons. */
    m_channelState.resize(m_channelCount);

    m_display->start();

    m_motionMaskBinData = (__m128i*) qMallocAligned(MD_WIDTH*MD_HEIGHT,32);
}

QnResourceWidget::~QnResourceWidget() {
    ensureAboutToBeDestroyedEmitted();

    delete m_display;

    if(!m_shadow.isNull()) {
        m_shadow.data()->setShapeProvider(NULL);
        delete m_shadow.data();
    }
    qFreeAligned(m_motionMaskBinData);
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

void QnResourceWidget::resizeEvent(QGraphicsSceneResizeEvent *event) {
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

void QnResourceWidget::drawMotionMask(QPainter *painter, const QRectF& rect) 
{
    QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource>(m_resource);
    if (!camera)
        return;
    drawFilledRegion(painter, rect, camera->getMotionMask(), QColor(255, 255, 255, 26));
}

void QnResourceWidget::prepareMotionMask()
{
    QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource>(m_resource);
    if (camera) 
    {
        m_motionMask = camera->getMotionMask();
        QnMetaDataV1::createMask(m_motionMask, m_motionMaskBinData);

        if (!m_motionMask.rects().isEmpty())
        {
            QVector<QRect> rects = m_motionMask.rects();
            for (int i = 0; i < rects.size(); ++i) {
                if (rects[i].x())
                    rects[i].setX(rects[i].x()+1);
                if (rects[i].y())
                    rects[i].setY(rects[i].y()+1);
            }
            m_motionMask.setRects(&rects[0], rects.size());
        }

    }
    m_motionMaskReady = true;
};

void QnResourceWidget::at_display_resourceUpdated()
{
    m_motionMaskReady = false;
}


void QnResourceWidget::drawMotionGrid(QPainter *painter, const QRectF& rect, const QnMetaDataV1Ptr &motion) {
    double xStep = rect.width() / (double) MD_WIDTH;
    double yStep = rect.height() / (double) MD_HEIGHT;

    if (!m_motionMaskReady)
        prepareMotionMask();

    painter->setPen(QPen(QColor(255, 255, 255, 40)));
    for (int x = 0; x < MD_WIDTH; ++x) 
    {
        if (m_motionMask.isEmpty())
        {
            painter->drawLine(QPointF(x*xStep, 0.0), QPointF(x*xStep, rect.height()));
        }
        else {
            QRegion lineRect(x, 0, 1, MD_HEIGHT+1);
            QRegion drawRegion = lineRect - m_motionMask.intersect(lineRect);
            foreach(const QRect& r, drawRegion.rects())
            {
                painter->drawLine(QPointF(x*xStep, r.top()*yStep), QPointF(x*xStep, r.bottom()*yStep));
            }
        }
    }
    for (int y = 0; y < MD_HEIGHT; ++y) {
        if (m_motionMask.isEmpty()) {
            painter->drawLine(QPointF(0.0, y*yStep), QPointF(rect.width(), y*yStep));
        }
        else {
            QRegion lineRect(0, y, MD_WIDTH+1, 1);
            QRegion drawRegion = lineRect - m_motionMask.intersect(lineRect);
            foreach(const QRect& r, drawRegion.rects())
            {
                painter->drawLine(QPointF(r.left()*xStep, y*yStep), QPointF(r.right()*xStep, y*yStep));
            }
        }
    }
    if (!motion)
        return;

    motion->removeMotion(m_motionMaskBinData);

    painter->setPen(QPen(QColor(255, 0, 0, 80)));
    for (int y = 0; y < MD_HEIGHT; ++y)
        for (int x = 0; x < MD_WIDTH; ++x)
            if(motion->isMotionAt(x, y))
                painter->drawRect(QRectF(QPointF(x*xStep, y*yStep), QPointF((x+1)*xStep, (y+1)*yStep)));
}

void QnResourceWidget::drawCurrentTime(QPainter *painter, const QRectF &rect, qint64 time)
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
        
        QnScopedPainterFontRollback fontRollback(painter, font);
        QnScopedPainterPenRollback penRollback(painter, QPen(QColor(255, 255, 255, 128)));
        painter->drawText(rect.width() - size.width()-4, rect.height() - size.height()+2, text);
    }
}

void QnResourceWidget::drawFilledRegion(QPainter *painter, const QRectF &rect, const QRegion &selection, const QColor& color) {
    qDebug() << selection;
    QPainterPath path;
    path.addRegion(selection);
    path = path.simplified(); // TODO: this is slow.

    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(rect.topLeft());
    painter->scale(rect.width() / MD_WIDTH, rect.height() / MD_HEIGHT);

    painter->setBrush(color);
    painter->setPen(QPen(color));
    painter->drawPath(path);
}

void QnResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/) {
    const QPaintEngine* paintEngine = painter->paintEngine();
    if (!paintEngine)
    {
        qnWarning("No OpenGL compatible paint engine is found.");
        return;
    }

    if (painter->paintEngine()->type() != QPaintEngine::OpenGL2 && painter->paintEngine()->type() != QPaintEngine::OpenGL) {
        qnWarning("Painting with the paint engine of type %1 is not supported", static_cast<int>(painter->paintEngine()->type()));
        return;
    }

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterFontRollback fontRollback(painter);

    /* Update screen size of a single channel. */
    QSizeF itemScreenSize = painter->combinedTransform().mapRect(boundingRect()).size();
    QSize channelScreenSize = QSizeF(itemScreenSize.width() / m_videoLayout->width(), itemScreenSize.height() / m_videoLayout->height()).toSize();
    if(channelScreenSize != m_channelScreenSize) {
        m_channelScreenSize = channelScreenSize;
        m_renderer->setChannelScreenSize(m_channelScreenSize);
    }

    qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();
    painter->beginNativePainting();
    for(int i = 0; i < m_channelCount; i++) {
        /* Draw content. */
        QRectF rect = channelRect(i);
        QnRenderStatus::RenderStatus status = m_renderer->paint(i, rect);

        /* Update channel state. */
        if(status == QnRenderStatus::RENDERED_NEW_FRAME)
            m_channelState[i].lastNewFrameTimeMSec = currentTimeMSec;

        /* Set overlay icon. */
        if(m_display->isPaused() && m_activityDecorationsVisible) {
            setOverlayIcon(i, PAUSED);
        } else if(status != QnRenderStatus::RENDERED_NEW_FRAME && (status != QnRenderStatus::RENDERED_OLD_FRAME || currentTimeMSec - m_channelState[i].lastNewFrameTimeMSec >= defaultLoadingTimeoutMSec) && !m_display->isPaused()) {
            setOverlayIcon(i, LOADING);
        } else {
            setOverlayIcon(i, NO_ICON);
        }

        /* Draw black rectangle if there is nothing to draw. */
        if(status != QnRenderStatus::RENDERED_OLD_FRAME && status != QnRenderStatus::RENDERED_NEW_FRAME) {
            glBegin(GL_QUADS);
            glColor4f(0.0, 0.0, 0.0, 1.0);
            glVertices(rect);
            glEnd();
        }

        /* Draw overlay icon. */
        drawOverlayIcon(i, rect);
    }
    painter->endNativePainting();

    /* Draw motion grid. */
    if (m_displayMotionGrid) {
        for(int i = 0; i < m_channelCount; i++) {
            QRectF rect = channelRect(i);

            drawMotionGrid(painter, rect, m_renderer->lastFrameMetadata(i));

            drawMotionMask(painter, rect);

            /* Selection. */
            if(!m_channelState[i].motionSelection.isEmpty())
                drawFilledRegion(painter, rect, m_channelState[i].motionSelection, QColor(0, 255, 0, 40));
        }
    }

    /* Draw current time. */
    qint64 time = m_renderer->lastDisplayedTime(0);
    if (time > 1000000ll * 3600 * 24)
        drawCurrentTime(painter, rect(), time); /* Do not show time for regular media files. */
}

void QnResourceWidget::setOverlayIcon(int channel, OverlayIcon icon) {
    ChannelState &state = m_channelState[channel];
    if(state.icon == icon)
        return;

    state.iconFadeInNeeded = state.icon == NO_ICON;
    state.iconChangeTimeMSec = QDateTime::currentMSecsSinceEpoch();
    state.icon = icon;
}

void QnResourceWidget::drawOverlayIcon(int channel, const QRectF &rect) {
    ChannelState &state = m_channelState[channel];
    if(state.icon == NO_ICON)
        return;

    qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();
    qreal fadeMultiplier = state.iconFadeInNeeded ? qBound(0.0, static_cast<qreal>(currentTimeMSec - state.iconChangeTimeMSec) / defaultOverlayFadeInDurationMSec, 1.0) : 1.0;

    glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.0, 0.0, 0.0, 0.5 * fadeMultiplier);
    glBegin(GL_QUADS);
    glVertices(rect);
    glEnd();

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
    switch(state.icon) {
    case LOADING:
        progressPainter()->paint(
            static_cast<qreal>(currentTimeMSec % defaultProgressPeriodMSec) / defaultProgressPeriodMSec,
            fadeMultiplier
        );
        break;
    case PAUSED:
        pausedPainter()->paint(0.5 * fadeMultiplier);
        break;
    default:
        break;
    }
    glPopMatrix();

    glPopAttrib();
}

void QnResourceWidget::paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
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

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true); /* Antialiasing is here for a reason. Without it border looks crappy. */
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

Qt::WindowFrameSection QnResourceWidget::windowFrameSectionAt(const QPointF &pos) const {
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

void QnResourceWidget::setMotionGridDisplayed(bool displayed) {
    if(m_displayMotionGrid == displayed)
        return;

    m_displayMotionGrid = displayed;

    m_display->archiveReader()->setSendMotion(displayed);
}

QPoint QnResourceWidget::mapToMotionGrid(const QPointF &itemPos) 
{
    QPointF pointf(cwiseDiv(itemPos, toPoint(cwiseDiv(size(), QSizeF(MD_WIDTH, MD_HEIGHT)))));
    QPoint p((int) (pointf.x()), (int) (pointf.y()));
    return bounded(p, QRect(0, 0, MD_WIDTH + 1, MD_HEIGHT + 1));
}

QPointF QnResourceWidget::mapFromMotionGrid(const QPoint &gridPos) {
    return cwiseMul(gridPos, toPoint(cwiseDiv(size(), QSizeF(MD_WIDTH, MD_HEIGHT))));
}

void QnResourceWidget::addToMotionSelection(const QRect &gridRect) {
    QRegion prevSelection = m_channelState[0].motionSelection;
    m_channelState[0].motionSelection += gridRect.intersected(QRect(0, 0, MD_WIDTH + 1, MD_HEIGHT + 1));
    if(prevSelection != m_channelState[0].motionSelection) {
        display()->archiveReader()->setMotionRegion(m_channelState[0].motionSelection);
        emit motionRegionSelected(m_resource, m_channelState[0].motionSelection);
    }
}

void QnResourceWidget::clearMotionSelection() {
    m_channelState[0].motionSelection = QRegion();
}

