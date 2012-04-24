#include "resource_widget.h"
#include <cassert>
#include <QPainter>
#include <QGraphicsLinearLayout>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/util.h>
#include <utils/common/synctime.h>

#include <core/resource/resource_media_layout.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <ui/common/color_transform.h>
#include <ui/animation/widget_opacity_animator.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <ui/graphics/painters/paused_painter.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/standard/graphicslabel.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

#include <camera/resource_display.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>

#include "camera/camdisplay.h"
#include "polygonal_shadow_item.h"
#include "resource_widget_renderer.h"
#include "settings.h"
#include "math.h"
#include "image_button_widget.h"
#include "viewport_bound_widget.h"
#include "utils/media/ffmpeg_helper.h"

namespace {

    /** Flashing text flash interval */
    static const int TEXT_FLASHING_PERIOD = 1000;

    /** Frame extension multiplier determines the width of frame extension relative
     * to frame width.
     *
     * Frame events are processed not only when hovering over the frame itself,
     * but also over its extension. */
    const qreal frameExtensionMultiplier = 1.0;

    /** Default shadow displacement, in scene coordinates. */
    const QPointF defaultShadowDisplacement = QPointF(qnGlobals->workbenchUnitSize(), qnGlobals->workbenchUnitSize()) * 0.05;

    /** Default timeout before the video is displayed as "loading", in milliseconds. */
    const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION;

    /** Default period of progress circle. */
    const qint64 defaultProgressPeriodMSec = 1000;

    /** Default duration of "fade-in" effect for overlay icons. */
    const qint64 defaultOverlayFadeInDurationMSec = 500;

    /** Default size of widget header buttons, in pixels. */
    const QSizeF headerButtonSize = QSizeF(24, 24);

    /** Background color for overlay panels. */
    const QColor overlayBackgroundColor = QColor(0, 0, 0, 96);

    const QColor overlayTextColor = QColor(255, 255, 255, 160);

    class QnLoadingProgressPainterFactory {
    public:
        QnLoadingProgressPainter *operator()(const QGLContext *) {
            return new QnLoadingProgressPainter(0.5, 12, 0.5, QColor(255, 255, 255, 0), QColor(255, 255, 255, 255));
        }
    };

    typedef QnGlContextData<QnLoadingProgressPainter, QnLoadingProgressPainterFactory> QnLoadingProgressPainterStorage;
    Q_GLOBAL_STATIC(QnLoadingProgressPainterStorage, qn_loadingProgressPainterStorage);

    /** Paused painter storage. */
    Q_GLOBAL_STATIC(QnGlContextData<QnPausedPainter>, qn_pausedPainterStorage);
}


// -------------------------------------------------------------------------- //
// Logic
// -------------------------------------------------------------------------- //
QnResourceWidget::QnResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_item(item),
    m_videoLayout(NULL),
    m_channelCount(0),
    m_renderer(NULL),
    m_aspectRatio(-1.0),
    m_enclosingAspectRatio(1.0),
    m_frameWidth(-1.0),
    m_frameOpacity(1.0),
    m_aboutToBeDestroyedEmitted(false),
    m_displayFlags(DISPLAY_SELECTION_OVERLAY | DISPLAY_BUTTONS),
    m_motionMaskValid(false),
    m_motionMaskBinDataValid(false),
    m_motionDrawType(DrawMaskOnly)
{
    /* Set up shadow. */
    m_shadow = new QnPolygonalShadowItem();
    QnPolygonalShadowItem *shadow = m_shadow.data();
    shadow->setParent(this);
    shadow->setColor(qnGlobals->shadowColor());
    shadow->setShapeProvider(this);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setShadowDisplacement(defaultShadowDisplacement);
    invalidateShadowShape();


    /* Set up frame. */
    setFrameWidth(0.0);


    /* Set up overlay widgets. */
    QFont font = this->font();
    font.setPixelSize(20);
    setFont(font);

    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::WindowText, overlayTextColor);
        setPalette(palette);
    }

    /* Header overlay. */
    m_headerTitleLabel = new GraphicsLabel();
    m_headerTitleLabel->setPerformanceHint(QStaticText::AggressiveCaching);

    qreal buttonSize = 24; /* In pixels. */

#if 0
    QnImageButtonWidget *togglePinButton = new QnImageButtonWidget();
    togglePinButton->setIcon(Skin::icon("decorations/pin.png", "decorations/unpin.png"));
    togglePinButton->setCheckable(true);
    togglePinButton->setChecked(item->isPinned());
    togglePinButton->setPreferredSize(headerButtonSize);
    connect(togglePinButton, SIGNAL(clicked()), item, SLOT(togglePinned()));
    headerLayout->addItem(togglePinButton);
#endif

    m_infoButton = new QnImageButtonWidget();
    m_infoButton->setIcon(qnSkin->icon("decorations/info.png"));
    m_infoButton->setPreferredSize(headerButtonSize);
    m_infoButton->setCheckable(true);
    connect(m_infoButton, SIGNAL(toggled(bool)), this, SLOT(fadeInfo(bool)));

    QnImageButtonWidget *closeButton = NULL;
    if(accessController()->permissions(item->layout()->resource()) & Qn::WritePermission) { // TODO: should autoupdate on permission changes
        closeButton = new QnImageButtonWidget();
        closeButton->setIcon(qnSkin->icon("decorations/close_item.png"));
        closeButton->setPreferredSize(headerButtonSize);
        connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
    }

    QGraphicsLinearLayout *headerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    headerLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    headerLayout->setSpacing(2.0);
    headerLayout->addItem(m_headerTitleLabel);
    headerLayout->addStretch(0x1000); /* Set large enough stretch for the buttons to be placed at the right end of the layout. */
    headerLayout->addItem(m_infoButton);
    if(closeButton)
        headerLayout->addItem(closeButton);

    QGraphicsWidget *headerWidget = new QGraphicsWidget();
    headerWidget->setLayout(headerLayout);
    headerWidget->setAcceptedMouseButtons(0);
    headerWidget->setAutoFillBackground(true);
    {
        QPalette palette = headerWidget->palette();
        palette.setColor(QPalette::Window, overlayBackgroundColor);
        headerWidget->setPalette(palette);
    }

    QGraphicsLinearLayout *headerOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    headerOverlayLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    headerOverlayLayout->addItem(headerWidget);
    headerOverlayLayout->addStretch(0x1000);

    m_headerOverlayWidget = new QnViewportBoundWidget(this);
    m_headerOverlayWidget->setLayout(headerOverlayLayout);
    m_headerOverlayWidget->setAcceptedMouseButtons(0);
    m_headerOverlayWidget->setOpacity(0.0);

    /* Footer overlay. */
    m_footerStatusLabel = new GraphicsLabel();

    m_footerTimeLabel = new GraphicsLabel();

    QGraphicsLinearLayout *footerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    footerLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    footerLayout->addItem(m_footerStatusLabel);
    footerLayout->addStretch(0x1000);
    footerLayout->addItem(m_footerTimeLabel);

    m_footerWidget = new QGraphicsWidget();
    m_footerWidget->setLayout(footerLayout);
    m_footerWidget->setAcceptedMouseButtons(0);
    m_footerWidget->setAutoFillBackground(true);
    {
        QPalette palette = m_footerWidget->palette();
        palette.setColor(QPalette::Window, overlayBackgroundColor);
        m_footerWidget->setPalette(palette);
    }
    m_footerWidget->setOpacity(0.0);

    QGraphicsLinearLayout *footerOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    footerOverlayLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    footerOverlayLayout->addStretch(0x1000);
    footerOverlayLayout->addItem(m_footerWidget);

    m_footerOverlayWidget = new QnViewportBoundWidget(this);
    m_footerOverlayWidget->setLayout(footerOverlayLayout);
    m_footerOverlayWidget->setAcceptedMouseButtons(0);
    m_footerOverlayWidget->setOpacity(0.0);

    connect(this, SIGNAL(updateOverlayTextLater()), this, SLOT(updateOverlayText()), Qt::QueuedConnection);


    /* Set up motion-related stuff. */
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_motionMaskBinData[i] = (__m128i*) qMallocAligned(MD_WIDTH * MD_HEIGHT/8, 32);
        memset(m_motionMaskBinData[i], 0, MD_WIDTH * MD_HEIGHT/8);
    }

    /* Set up video rendering. */
    m_resource = qnResPool->getResourceByUniqId(item->resourceUid());
    m_display = new QnResourceDisplay(m_resource, this);
    connect(m_resource.data(), SIGNAL(resourceChanged()), this, SLOT(at_resource_resourceChanged()));
    connect(m_resource.data(), SIGNAL(nameChanged()), this, SLOT(at_resource_nameChanged()));
    connect(m_display->camDisplay(), SIGNAL(stillImageChanged()), this, SLOT(at_camDisplay_stillImageChanged()));

    Q_ASSERT(m_display);
    m_videoLayout = m_display->videoLayout();
    Q_ASSERT(m_videoLayout);
    m_channelCount = m_videoLayout->numberOfChannels();

    m_renderer = new QnResourceWidgetRenderer(m_channelCount);
    connect(m_renderer, SIGNAL(sourceSizeChanged(const QSize &)), this, SLOT(at_sourceSizeChanged(const QSize &)));
    m_display->addRenderer(m_renderer);

    /* Init static text. */
    m_noDataStaticText.setText(tr("NO DATA"));
    m_noDataStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_offlineStaticText.setText(tr("NO SIGNAL"));
    m_offlineStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticText.setText(tr("UNAUTHORIZED"));
    m_unauthorizedStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticText2.setText(tr("Please check authentication information in camera settings"));
    m_unauthorizedStaticText2.setPerformanceHint(QStaticText::AggressiveCaching);

    for (int i = 0; i < 10; ++i) {
        m_sensStaticText[i].setText(QString::number(i));
        m_sensStaticText[i].setPerformanceHint(QStaticText::AggressiveCaching);
    }

    /* Set up overlay icons. */
    m_channelState.resize(m_channelCount);

    at_resource_nameChanged();
    at_camDisplay_stillImageChanged();

}


QnResourceWidget::~QnResourceWidget() {
    ensureAboutToBeDestroyedEmitted();

    delete m_display;

    if(!m_shadow.isNull()) {
        m_shadow.data()->setShapeProvider(NULL);
        delete m_shadow.data();
    }

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        qFreeAligned(m_motionMaskBinData[i]);
}

const QnResourcePtr &QnResourceWidget::resource() const {
    return m_display->resource();
}

void QnResourceWidget::setFrameWidth(qreal frameWidth) {
    if(qFuzzyCompare(m_frameWidth, frameWidth))
        return;

    prepareGeometryChange();

    m_frameWidth = frameWidth;
    qreal extendedFrameWidth = m_frameWidth * (1.0 + frameExtensionMultiplier);
    setWindowFrameMargins(extendedFrameWidth, extendedFrameWidth, extendedFrameWidth, extendedFrameWidth);

    invalidateShadowShape();
    if(m_shadow.data() != NULL)
        m_shadow.data()->setSoftWidth(m_frameWidth);
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

    QSizeF oldSize = size();

    base_type::setGeometry(geometry);
    setTransformOriginPoint(rect().center());

    m_headerOverlayWidget->setDesiredSize(size());
    m_footerOverlayWidget->setDesiredSize(size());
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
    setDisplayFlag(DISPLAY_ACTIVITY_OVERLAY, true);
}

void QnResourceWidget::hideActivityDecorations() {
    setDisplayFlag(DISPLAY_ACTIVITY_OVERLAY, false);
}

void QnResourceWidget::fadeOutOverlay() {
    opacityAnimator(m_footerOverlayWidget, 1.0)->animateTo(0.0);
    opacityAnimator(m_headerOverlayWidget, 1.0)->animateTo(0.0);
}

void QnResourceWidget::fadeInOverlay() {
    opacityAnimator(m_footerOverlayWidget, 1.0)->animateTo(1.0);
    opacityAnimator(m_headerOverlayWidget, 1.0)->animateTo(1.0);
}

void QnResourceWidget::fadeOutInfo() {
    opacityAnimator(m_footerWidget, 1.0)->animateTo(0.0);
}

void QnResourceWidget::fadeInInfo() {
    updateOverlayText();
    
    opacityAnimator(m_footerWidget, 1.0)->animateTo(1.0);
}

void QnResourceWidget::fadeInfo(bool fadeIn) {
    if(fadeIn) {
        fadeInInfo();
    } else {
        fadeOutInfo();
    }
}

void QnResourceWidget::invalidateMotionMask() {
    m_motionMaskValid = false;
}

void QnResourceWidget::addToMotionRegion(int sens, const QRect& rect, int channel) {
    ensureMotionMask();

    m_motionRegionList[channel].addRect(sens, rect);

    invalidateMotionMaskBinData();
}

void QnResourceWidget::clearMotionRegions() {
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionRegionList[i] = QnMotionRegion();
    m_motionMaskValid = true;

    invalidateMotionMaskBinData();
}

void QnResourceWidget::ensureMotionMask()
{
    if(m_motionMaskValid)
        return;

    QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource>(m_resource);
    if (camera)
    {
        m_motionRegionList = camera->getMotionRegionList();

    }
    m_motionMaskValid = true;
}

void QnResourceWidget::ensureMotionMaskBinData() {
    if(m_motionMaskBinDataValid)
        return;

    ensureMotionMask();
    Q_ASSERT(((unsigned long)m_motionMaskBinData[0])%16 == 0);
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        QnMetaDataV1::createMask(m_motionRegionList[i].getMotionMask(), (char*)m_motionMaskBinData[i]);
}

void QnResourceWidget::invalidateMotionMaskBinData() {
    m_motionMaskBinDataValid = false;
}

void QnResourceWidget::setOverlayIcon(int channel, OverlayIcon icon) {
    ChannelState &state = m_channelState[channel];
    if(state.icon == icon)
        return;

    state.iconFadeInNeeded = state.icon == NO_ICON;
    state.iconChangeTimeMSec = qnSyncTime->currentMSecsSinceEpoch();
    state.icon = icon;
}

Qt::WindowFrameSection QnResourceWidget::windowFrameSectionAt(const QPointF &pos) const {
    return Qn::toQtFrameSection(static_cast<Qn::WindowFrameSection>(static_cast<int>(windowFrameSectionsAt(QRectF(pos, QSizeF(0.0, 0.0))))));
}

Qn::WindowFrameSections QnResourceWidget::windowFrameSectionsAt(const QRectF &region) const {
    Qn::WindowFrameSections result = Qn::calculateRectangularFrameSections(windowFrameRect(), rect(), region);

    /* This widget has no side frame sections in case aspect ratio is set. */
    if(hasAspectRatio())
        result = result & ~(Qn::LeftSection | Qn::RightSection | Qn::TopSection | Qn::BottomSection);

    return result;
}

void QnResourceWidget::ensureAboutToBeDestroyedEmitted() {
    if(m_aboutToBeDestroyedEmitted)
        return;

    m_aboutToBeDestroyedEmitted = true;
    emit aboutToBeDestroyed();
}

int QnResourceWidget::motionGridWidth() const
{
    return MD_WIDTH * m_videoLayout->width();
}

int QnResourceWidget::motionGridHeight() const
{
    return MD_HEIGHT * m_videoLayout->height();
}

QPoint QnResourceWidget::mapToMotionGrid(const QPointF &itemPos)
{
    QPointF gridPosF(cwiseDiv(itemPos, toPoint(cwiseDiv(size(), QSizeF(motionGridWidth(), motionGridHeight())))));
    QPoint gridPos(qFuzzyFloor(gridPosF.x()), qFuzzyFloor(gridPosF.y()));

    return bounded(gridPos, QRect(0, 0, motionGridWidth() + 1, motionGridHeight() + 1));
}

QPointF QnResourceWidget::mapFromMotionGrid(const QPoint &gridPos) {
    return cwiseMul(gridPos, toPoint(cwiseDiv(size(), QSizeF(motionGridWidth(), motionGridHeight()))));
}

void QnResourceWidget::addToMotionSelection(const QRect &gridRect) 
{
    QList<QRegion> prevSelection;
    QList<QRegion> newSelection;

    for (int i = 0; i < m_channelState.size(); ++i)
    {
        prevSelection << m_channelState[i].motionSelection;

        QRect r(0, 0, MD_WIDTH, MD_HEIGHT);
        r.translate(m_videoLayout->h_position(i)*MD_WIDTH, m_videoLayout->v_position(i)*MD_HEIGHT);
        r = gridRect.intersected(r);

        if (r.width() > 0 && r.height() > 0) 
        {
            QRegion r1(r);
            r1.translate(-m_videoLayout->h_position(i)*MD_WIDTH, -m_videoLayout->v_position(i)*MD_HEIGHT);
            r1 -= m_motionRegionList[i].getMotionMask();

            m_channelState[i].motionSelection += r1;
        }

        newSelection << m_channelState[i].motionSelection;
    }

    if(prevSelection != newSelection && display()->archiveReader())
        emit motionRegionSelected(m_resource, display()->archiveReader(), newSelection);
}

void QnResourceWidget::clearMotionSelection() 
{
    bool allEmpty = true;
    for (int i = 0; i < m_channelState.size(); ++i) 
        allEmpty &= m_channelState[i].motionSelection.isEmpty();
    if (allEmpty)
        return;

    QList<QRegion> rez;
    for (int i = 0; i < m_channelState.size(); ++i) {
        m_channelState[i].motionSelection = QRegion();
        rez << QRegion();
    }
    if (display()->archiveReader())
        emit motionRegionSelected(m_resource, display()->archiveReader(), rez);
}

void QnResourceWidget::setDisplayFlags(DisplayFlags flags) {
    if(m_displayFlags == flags)
        return;

    DisplayFlags changedFlags = m_displayFlags ^ flags;
    m_displayFlags = flags;

    if(changedFlags & DISPLAY_MOTION_GRID) {
        QnAbstractArchiveReader *reader = m_display->archiveReader();
        if (reader)
            reader->setSendMotion(flags & DISPLAY_MOTION_GRID);
    }

    if(changedFlags & DISPLAY_BUTTONS)
        m_headerOverlayWidget->setVisible(flags & DISPLAY_BUTTONS);

    emit displayFlagsChanged();
}

void QnResourceWidget::updateOverlayText() {
    qint64 time = m_renderer->lastDisplayedTime(0);
    if (time > 1000000ll * 3600 * 24) /* Do not show time for regular media files. */
        m_footerTimeLabel->setText(QDateTime::fromMSecsSinceEpoch(time/1000).toString(tr("hh:mm:ss.zzz")));

    qreal fps = 0.0;
    qreal mbps = 0.0;
    for(int i = 0; i < m_channelCount; i++) {
        const QnStatistics *statistics = m_display->mediaProvider()->getStatistics(i);
        fps = qMax(fps, static_cast<qreal>(statistics->getFrameRate()));
        mbps += statistics->getBitrate();
    }

    QString codecName;
    QnMediaContextPtr codec = m_display->mediaProvider()->getCodecContext();
    if (codec && codec->ctx()) 
        codecName = codecIDToString(codec->ctx()->codec_id);

    m_footerStatusLabel->setText(tr("%1fps @ %2Mbps (%3)").arg(fps, 0, 'g', 2).arg(mbps, 0, 'g', 2).arg(codecName));
}


// -------------------------------------------------------------------------- //
// Shadow
// -------------------------------------------------------------------------- //
void QnResourceWidget::setShadowDisplacement(const QPointF &displacement) {
    m_shadowDisplacement = displacement;

    updateShadowPos();
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
        /* Shadow Z value is managed by workbench display. */
        // m_shadow.data()->setZValue(zValue());
        m_shadow.data()->stackBefore(this);
    }
}

void QnResourceWidget::updateShadowPos() {
    if(!m_shadow.isNull())
        m_shadow.data()->setPos(mapToScene(0.0, 0.0) + m_shadowDisplacement);
}

void QnResourceWidget::updateShadowOpacity() {
    if(!m_shadow.isNull())
        m_shadow.data()->setOpacity(opacity());
}

void QnResourceWidget::updateShadowVisibility() {
    if(!m_shadow.isNull())
        m_shadow.data()->setVisible(isVisible());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
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
        if(scene() && !m_shadow.isNull()) {
            scene()->addItem(m_shadow.data());
            updateShadowZ();
            updateShadowPos();
        }
        break;
    case ItemOpacityHasChanged:
        updateShadowOpacity();
        break;
    case ItemZValueHasChanged:
        updateShadowZ();
        break;
    case ItemVisibleHasChanged:
        updateShadowVisibility();
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

void QnResourceWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    fadeInOverlay();

    base_type::hoverEnterEvent(event);
}

void QnResourceWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if(qFuzzyIsNull(m_headerOverlayWidget->opacity()))
        fadeInOverlay();

    base_type::hoverMoveEvent(event);
}

void QnResourceWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    if(!m_infoButton->isChecked())
        fadeOutOverlay();

    base_type::hoverLeaveEvent(event);
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

void QnResourceWidget::at_resource_resourceChanged() {
    invalidateMotionMask();
}

void QnResourceWidget::at_resource_nameChanged() {
    m_headerTitleLabel->setText(m_resource->getName());
}

void QnResourceWidget::at_camDisplay_stillImageChanged() {
    m_infoButton->setVisible(!display()->camDisplay()->isStillImage());
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnResourceWidget::drawFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset) {
    qreal unit = qnGlobals->workbenchUnitSize();

    QFont font;
    font.setPointSizeF(textSize * unit);
    font.setStyleHint(QFont::SansSerif, QFont::ForceOutline);
    QnScopedPainterFontRollback fontRollback(painter, font);
    QnScopedPainterPenRollback penRollback(painter, QPen(QColor(255, 208, 208, 196)));
    
    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * qAbs(sin(qnSyncTime->currentMSecsSinceEpoch() / qreal(TEXT_FLASHING_PERIOD * 2) * M_PI)));
    painter->drawStaticText(rect().center() - toPoint(text.size() / 2) + offset * unit, text);
    painter->setOpacity(opacity);
}

void QnResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/) {
    if (painter->paintEngine() == NULL) {
        qnWarning("No OpenGL-compatible paint engine was found.");
        return;
    }

    if (painter->paintEngine()->type() != QPaintEngine::OpenGL2 && painter->paintEngine()->type() != QPaintEngine::OpenGL) {
        qnWarning("Painting with the paint engine of type '%1' is not supported", static_cast<int>(painter->paintEngine()->type()));
        return;
    }

    if(m_pausedPainter.isNull()) {
        m_pausedPainter = qn_pausedPainterStorage()->get();
        m_loadingProgressPainter = qn_loadingProgressPainterStorage()->get();
        //m_noDataPainter = qn_noDataPainterStorage()->get();
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

    qint64 currentTimeMSec = qnSyncTime->currentMSecsSinceEpoch();
    painter->beginNativePainting();
    //glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for(int i = 0; i < m_channelCount; i++) {
        /* Draw content. */
        QRectF rect = channelRect(i);
        m_renderStatus = m_renderer->paint(i, rect, effectiveOpacity());

        /* Draw black rectangle if nothing was drawn. */
        if(m_renderStatus != Qn::OldFrameRendered && m_renderStatus != Qn::NewFrameRendered) {
            glColor4f(0.0, 0.0, 0.0, effectiveOpacity());
            glBegin(GL_QUADS);
            glVertices(rect);
            glEnd();
        }

        /* Update channel state. */
        if(m_renderStatus == Qn::NewFrameRendered)
            m_channelState[i].lastNewFrameTimeMSec = currentTimeMSec;

        /* Set overlay icon. */
        if (m_display->camDisplay()->isStillImage()) {
            setOverlayIcon(i, NO_ICON);
        } else if(m_display->isPaused() && (m_displayFlags & DISPLAY_ACTIVITY_OVERLAY)) {
            setOverlayIcon(i, PAUSED);
        } else if (m_display->camDisplay()->isRealTimeSource() && m_display->resource()->getStatus() == QnResource::Offline) {
            setOverlayIcon(i, OFFLINE);
        } else if (m_display->camDisplay()->isRealTimeSource() && m_display->resource()->getStatus() == QnResource::Unauthorized) {
            setOverlayIcon(i, UNAUTHORIZED);
        } else if (m_display->camDisplay()->isNoData()) {
            setOverlayIcon(i, NO_DATA); 
        } else if(m_renderStatus != Qn::NewFrameRendered && (m_renderStatus != Qn::OldFrameRendered || currentTimeMSec - m_channelState[i].lastNewFrameTimeMSec >= defaultLoadingTimeoutMSec) && !m_display->isPaused()) {
            setOverlayIcon(i, LOADING);
        } else {
            setOverlayIcon(i, NO_ICON);
        }

        /* Draw overlay icon. */
        drawOverlayIcon(i, rect);

        /* Draw selected / not selected overlay. */
        drawSelection(rect);
    }
    
    glDisable(GL_BLEND);
    //glPopAttrib();
    painter->endNativePainting();

    for(int i = 0; i < m_channelCount; i++) 
    {
        if (m_channelState[i].icon == NO_DATA) {
            drawFlashingText(painter, m_noDataStaticText, 0.1);
        } else if (m_channelState[i].icon == OFFLINE) {
            drawFlashingText(painter, m_offlineStaticText, 0.1);
        } else if (m_channelState[i].icon == UNAUTHORIZED) {
            drawFlashingText(painter, m_unauthorizedStaticText, 0.1);
            drawFlashingText(painter, m_unauthorizedStaticText2, 0.025, QPointF(0.0, 0.1));
        }
    }

    /* Draw motion grid. */
    if (m_displayFlags & DISPLAY_MOTION_GRID) {
        for(int i = 0; i < m_channelCount; i++) 
        {
            QRectF rect = channelRect(i);

            drawMotionGrid(painter, rect, m_renderer->lastFrameMetadata(i), i);

            drawMotionMask(painter, rect, i);

            /* Selection. */
            if(!m_channelState[i].motionSelection.isEmpty()) {
				//drawFilledRegion(painter, rect, m_channelState[i].motionSelection, subColor(qnGlobals->mrsColor(), qnGlobals->selectionOpacityDelta()));
                QColor clr = subColor(qnGlobals->mrsColor(), QColor(0,0,0, 0xa0));
                drawFilledRegion(painter, rect, m_channelState[i].motionSelection, clr, clr);
            }
        }
    }

    if(m_footerOverlayWidget->isVisible() && !qFuzzyIsNull(m_footerOverlayWidget->opacity())) 
        emit updateOverlayTextLater();
}

void QnResourceWidget::paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(qFuzzyIsNull(m_frameOpacity))
        return;

    QSizeF size = this->size();
    qreal w = size.width();
    qreal h = size.height();
    qreal fw = m_frameWidth;
    QColor color = palette().color(isSelected() ? QPalette::Active : QPalette::Inactive, QPalette::Shadow);

    QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_frameOpacity);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true); /* Antialiasing is here for a reason. Without it border looks crappy. */
    painter->fillRect(QRectF(-fw,     -fw,     w + fw * 2,  fw), color);
    painter->fillRect(QRectF(-fw,     h,       w + fw * 2,  fw), color);
    painter->fillRect(QRectF(-fw,     0,       fw,          h),  color);
    painter->fillRect(QRectF(w,       0,       fw,          h),  color);
}

void QnResourceWidget::drawSelection(const QRectF &rect) {
    if(!isSelected())
        return;

    if(!(m_displayFlags & DISPLAY_SELECTION_OVERLAY))
        return;

    QColor color = qnGlobals->selectionColor();
    color.setAlpha(color.alpha() * effectiveOpacity());
    glColor(color);
    glBegin(GL_QUADS);
    glVertices(rect);
    glEnd();
}

void QnResourceWidget::drawOverlayIcon(int channel, const QRectF &rect) {
    ChannelState &state = m_channelState[channel];
    if(state.icon == NO_ICON)
        return;

    qint64 currentTimeMSec = qnSyncTime->currentMSecsSinceEpoch();
    qreal opacityMultiplier = effectiveOpacity() * (state.iconFadeInNeeded ? qBound(0.0, static_cast<qreal>(currentTimeMSec - state.iconChangeTimeMSec) / defaultOverlayFadeInDurationMSec, 1.0) : 1.0);

    glColor4f(0.0, 0.0, 0.0, 0.5 * opacityMultiplier);
    glBegin(GL_QUADS);
    glVertices(rect);
    glEnd();

    if(state.icon == NO_DATA)
        return;

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
        m_loadingProgressPainter->paint(
            static_cast<qreal>(currentTimeMSec % defaultProgressPeriodMSec) / defaultProgressPeriodMSec,
            opacityMultiplier
        );
        break;
    case PAUSED:
        m_pausedPainter->paint(0.5 * opacityMultiplier);
        break;
    default:
        break;
    }
    glPopMatrix();
}

void QnResourceWidget::drawMotionGrid(QPainter *painter, const QRectF& rect, const QnMetaDataV1Ptr &motion, int channel) {
    double xStep = rect.width() / (double) MD_WIDTH;
    double yStep = rect.height() / (double) MD_HEIGHT;

    ensureMotionMask();

    QVector<QPointF> gridLines;

    for (int x = 0; x < MD_WIDTH; ++x)
    {
        if (m_motionRegionList[channel].isEmpty())
        {
            gridLines << QPointF(x*xStep, 0.0) << QPointF(x*xStep, rect.height());
        }
        else {
            QRegion lineRect(x, 0, 1, MD_HEIGHT+1);
            QRegion drawRegion = lineRect - m_motionRegionList[channel].getMotionMask().intersect(lineRect);
            foreach(const QRect& r, drawRegion.rects())
            {
                gridLines << QPointF(x*xStep, r.top()*yStep) << QPointF(x*xStep, qMin(rect.height(),(r.top()+r.height())*yStep));
            }
        }
    }

    for (int y = 0; y < MD_HEIGHT; ++y) {
        if (m_motionRegionList[channel].isEmpty()) {
            gridLines << QPointF(0.0, y*yStep) << QPointF(rect.width(), y*yStep);
        }
        else {
            QRegion lineRect(0, y, MD_WIDTH+1, 1);
            QRegion drawRegion = lineRect - m_motionRegionList[channel].getMotionMask().intersect(lineRect);
            foreach(const QRect& r, drawRegion.rects())
            {
                gridLines << QPointF(r.left()*xStep, y*yStep) << QPointF(qMin(rect.width(), (r.left()+r.width())*xStep), y*yStep);
            }
        }
    }

    QnScopedPainterTransformRollback transformRollback(painter);
    painter->setPen(QPen(QColor(255, 255, 255, 40)));
    painter->translate(rect.topLeft());
    painter->drawLines(gridLines);

    if (!motion || motion->channelNumber != channel)
        return;

    ensureMotionMaskBinData();

    QPainterPath motionPath;
    motion->removeMotion(m_motionMaskBinData[motion->channelNumber]);
    for (int y = 0; y < MD_HEIGHT; ++y)
        for (int x = 0; x < MD_WIDTH; ++x)
            if(motion->isMotionAt(x, y))
                motionPath.addRect(QRectF(QPointF(x*xStep, y*yStep), QPointF((x+1)*xStep, (y+1)*yStep)));
    QPen pen(QColor(0xff, 0, 0, 128));
    pen.setWidth(9);
    painter->setPen(pen);
    painter->drawPath(motionPath);
}

void QnResourceWidget::drawFilledRegion(QPainter *painter, const QRectF &rect, const QRegion &selection, const QColor &color, const QColor &penColor) {
    QPainterPath path;
    path.addRegion(selection);
    path = path.simplified(); // TODO: this is slow.

    QnScopedPainterTransformRollback transformRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter, color);
    painter->translate(rect.topLeft());
    painter->scale(rect.width() / MD_WIDTH, rect.height() / MD_HEIGHT);
    painter->setPen(QPen(penColor));
    painter->drawPath(path);
}

void QnResourceWidget::drawMotionSensitivity(QPainter *painter, const QRectF &rect, const QnMotionRegion& region, int channel)
{
    double xStep = rect.width() / (double) MD_WIDTH;
    double yStep = rect.height() / (double) MD_HEIGHT;

    painter->setPen(Qt::black);
    QFont font;
    qreal unit = qnGlobals->workbenchUnitSize();
    font.setPointSizeF(0.03 * unit);
    painter->setFont(font);

    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        foreach(const QRect& rect, region.getRegionBySens(sens).rects())
        {
            if (rect.width() < 2 || rect.height() < 2)
                continue;
            for (int x = rect.left(); x <= rect.right(); ++x)
            {
                for (int y = rect.top(); y <= rect.bottom(); ++y)
                {
                    painter->drawStaticText(x*xStep + xStep*0.1, y*yStep + yStep*0.1, m_sensStaticText[sens]);
                    break;
                }
                break;
            }
        }
    }
}

void QnResourceWidget::drawMotionMask(QPainter *painter, const QRectF &rect, int channel)
{
    ensureMotionMask();
    if (m_motionDrawType == DrawAllMotionInfo) 
    {
        QnMotionRegion& r = m_motionRegionList[channel];
        QRegion reg;
        for (int i = 0; i < 10; ++i) 
        {
            QColor clr = i > 0 ? QColor(150 +  i*4, 16*(10-i), 0, 96+i*4) : qnGlobals->motionMaskColor();
            QRegion reg = m_motionRegionList[channel].getRegionBySens(i);
            if (i > 0)
                reg -= m_motionRegionList[channel].getRegionBySens(0);
            drawFilledRegion(painter, rect, reg, clr, Qt::black);
        }

        drawMotionSensitivity(painter, rect, m_motionRegionList[channel], channel);
    }
    else {
        drawFilledRegion(painter, rect, m_motionRegionList[channel].getMotionMask(), qnGlobals->motionMaskColor(), qnGlobals->motionMaskColor());
    }
}

void QnResourceWidget::setDrawMotionWindows(MotionDrawType value)
{
    m_motionDrawType = value;   
}

QList<QnMotionRegion>& QnResourceWidget::getMotionRegionList()
{
    return m_motionRegionList;
}
