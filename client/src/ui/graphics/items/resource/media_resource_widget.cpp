#include "media_resource_widget.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtGui/QAction>

#include <plugins/resources/archive/abstract_archive_stream_reader.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/settings.h>

#include <core/resource/media_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>

#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/help/help_topics.h>
#include <ui/common/color_transformations.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include "resource_widget_renderer.h"
#include "resource_widget.h"


// TODO: remove
#include "camera/caching_time_period_loader.h"
#include "ui/workbench/workbench_navigator.h"

#define QN_MEDIA_RESOURCE_WIDGET_SHOW_HI_LO_RES

namespace {
    template<class T>
    void resizeList(QList<T> &list, int size) {
        while(list.size() < size)
            list.push_back(T());
        while(list.size() > size)
            list.pop_back();
    }

} // anonymous namespace



QnMediaResourceWidget::QnMediaResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    QnResourceWidget(context, item, parent),
    m_resolutionMode(Qn::AutoResolution),
    m_motionSensitivityValid(false),
    m_binaryMotionMaskValid(false)
{
    m_resource = base_type::resource().dynamicCast<QnMediaResource>();
    if(!m_resource) 
        qnCritical("Media resource widget was created with a non-media resource.");
    m_camera = m_resource.dynamicCast<QnVirtualCameraResource>();

    /* Set up video rendering. */
    m_display = new QnResourceDisplay(m_resource, this);
    connect(m_resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)), this, SLOT(at_resource_resourceChanged()));
    connect(m_display->camDisplay(), SIGNAL(stillImageChanged()), this, SLOT(updateButtonsVisibility()));
    connect(m_display->camDisplay(), SIGNAL(liveMode(bool)), this, SLOT(at_camDisplay_liveChanged()));
    setChannelLayout(m_display->videoLayout());

    const QGLWidget *viewPortAsGLWidget = qobject_cast<const QGLWidget *>(QnWorkbenchContextAware::display()->view()->viewport());
    m_renderer = new QnResourceWidgetRenderer(channelCount(), NULL, viewPortAsGLWidget ? viewPortAsGLWidget->context() : NULL);
    connect(m_renderer, SIGNAL(sourceSizeChanged(const QSize &)), this, SLOT(at_renderer_sourceSizeChanged(const QSize &)));
    m_display->addRenderer(m_renderer);

    /* Set up static text. */
    for (int i = 0; i < 10; ++i) {
        m_sensStaticText[i].setText(QString::number(i));
        m_sensStaticText[i].setPerformanceHint(QStaticText::AggressiveCaching);
    }

    /* Set up info updates. */
    connect(this, SIGNAL(updateInfoTextLater()), this, SLOT(updateInfoText()), Qt::QueuedConnection);
    updateInfoText();

    /* Set up buttons. */
    QnImageButtonWidget *radassButton = new QnImageButtonWidget();
    radassButton->setProperty(Qn::NoBlockMotionSelection, true);
    radassButton->setToolTip(tr("Radass mode"));
    connect(radassButton, SIGNAL(clicked()), this, SLOT(at_radassButton_clicked()));

    QnImageButtonWidget *searchButton = new QnImageButtonWidget();
    searchButton->setIcon(qnSkin->icon("item/search.png"));
    searchButton->setCheckable(true);
    searchButton->setProperty(Qn::NoBlockMotionSelection, true);
    searchButton->setToolTip(tr("Smart Search"));
    setHelpTopic(searchButton, Qn::MainWindow_MediaItem_SmartSearch_Help);
    connect(searchButton, SIGNAL(toggled(bool)), this, SLOT(at_searchButton_toggled(bool)));

    QnImageButtonWidget *ptzButton = new QnImageButtonWidget();
    ptzButton->setIcon(qnSkin->icon("item/ptz.png"));
    ptzButton->setCheckable(true);
    ptzButton->setProperty(Qn::NoBlockMotionSelection, true);
    ptzButton->setToolTip(tr("PTZ"));
    setHelpTopic(ptzButton, Qn::MainWindow_MediaItem_Ptz_Help);
    connect(ptzButton, SIGNAL(toggled(bool)), this, SLOT(at_ptzButton_toggled(bool)));
    connect(ptzButton, SIGNAL(toggled(bool)), this, SLOT(updateButtonsVisibility()));

    buttonBar()->addButton(RadassButton, radassButton);
    buttonBar()->addButton(MotionSearchButton, searchButton);
    buttonBar()->addButton(PtzButton, ptzButton);
    
    if(m_camera) {
        QTimer *timer = new QTimer(this);
        
        connect(timer,              SIGNAL(timeout()),                                                  this,   SLOT(updateIconButton()));
        connect(context->instance<QnWorkbenchServerTimeWatcher>(), SIGNAL(offsetsChanged()),            this,   SLOT(updateIconButton()));
        connect(m_camera.data(),    SIGNAL(statusChanged(const QnResourcePtr &)),                       this,   SLOT(updateIconButton()));
        connect(m_camera.data(),    SIGNAL(scheduleTasksChanged(const QnSecurityCamResourcePtr &)),     this,   SLOT(updateIconButton()));
        connect(m_camera.data(),    SIGNAL(cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &)),this,   SLOT(updateButtonsVisibility()));

        timer->start(1000 * 60); /* Update icon button every minute. */
    }

    updateButtonsVisibility();
    at_camDisplay_liveChanged();
    updateIconButton();
    updateRadassButton();
}

QnMediaResourceWidget::~QnMediaResourceWidget() {
    ensureAboutToBeDestroyedEmitted();

    delete m_display;

    foreach(__m128i *data, m_binaryMotionMask)
        qFreeAligned(data);
    m_binaryMotionMask.clear();
}

QnMediaResourcePtr QnMediaResourceWidget::resource() const {
    return m_resource;
}

int QnMediaResourceWidget::motionGridWidth() const {
    return MD_WIDTH * channelLayout()->width();
}

int QnMediaResourceWidget::motionGridHeight() const {
    return MD_HEIGHT * channelLayout()->height();
}

QPoint QnMediaResourceWidget::mapToMotionGrid(const QPointF &itemPos) {
    QPointF gridPosF(cwiseDiv(itemPos, toPoint(cwiseDiv(size(), QSizeF(motionGridWidth(), motionGridHeight())))));
    QPoint gridPos(qFuzzyFloor(gridPosF.x()), qFuzzyFloor(gridPosF.y()));

    return bounded(gridPos, QRect(0, 0, motionGridWidth(), motionGridHeight()));
}

QPointF QnMediaResourceWidget::mapFromMotionGrid(const QPoint &gridPos) {
    return cwiseMul(gridPos, toPoint(cwiseDiv(size(), QSizeF(motionGridWidth(), motionGridHeight()))));
}

QPoint QnMediaResourceWidget::channelGridOffset(int channel) const {
    return QPoint(channelLayout()->h_position(channel) * MD_WIDTH, channelLayout()->v_position(channel) * MD_HEIGHT);
}

const QList<QRegion> &QnMediaResourceWidget::motionSelection() const {
    return m_motionSelection;
}

bool QnMediaResourceWidget::isMotionSelectionEmpty() const {
    foreach(const QRegion &region, m_motionSelection)
        if(!region.isEmpty())
            return false;
    return true;
}

void QnMediaResourceWidget::addToMotionSelection(const QRect &gridRect) {
    ensureMotionSensitivity();

    bool changed = false;

    for (int i = 0; i < channelCount(); ++i) {
        QRect rect = gridRect.translated(-channelGridOffset(i)).intersected(QRect(0, 0, MD_WIDTH, MD_HEIGHT));
        if (rect.isEmpty()) 
            continue;
        
        QRegion selection;
        selection += rect;
        selection -= m_motionSensitivity[i].getMotionMask();

        if(!selection.isEmpty()) {
            if(changed) {
                /* In this case we don't need to bother comparing old & new selection regions. */
                m_motionSelection[i] += selection; 
            } else {
                QRegion oldSelection = m_motionSelection[i];
                m_motionSelection[i] += selection;
                changed |= (oldSelection != m_motionSelection[i]);
            }
        }
    }

    if(changed){
        invalidateMotionSelectionCache();
        emit motionSelectionChanged();
    }
}

void QnMediaResourceWidget::clearMotionSelection() {
    if(isMotionSelectionEmpty())
        return;

    for (int i = 0; i < m_motionSelection.size(); ++i)
        m_motionSelection[i] = QRegion();

    invalidateMotionSelectionCache();
    emit motionSelectionChanged();
}

void QnMediaResourceWidget::invalidateMotionSensitivity() {
    m_motionSensitivityValid = false;
}

void QnMediaResourceWidget::ensureMotionSensitivity() const {
    if(m_motionSensitivityValid)
        return;

    if (m_camera) {
        m_motionSensitivity = m_camera->getMotionRegionList();

        if(m_motionSensitivity.size() != channelCount()) {
            qnWarning("Camera '%1' returned a motion sensitivity list of invalid size.", m_camera->getName());
            resizeList(m_motionSensitivity, channelCount());
        }
    } else if(m_resource->flags() & QnResource::motion) {
        for(int i = 0, count = channelCount(); i < count; i++)
            m_motionSensitivity.push_back(QnMotionRegion());
    } else {
        m_motionSensitivity.clear();
    }

    m_motionSensitivityValid = true;
}

bool QnMediaResourceWidget::addToMotionSensitivity(const QRect &gridRect, int sensitivity) {
    ensureMotionSensitivity();

    bool changed = false;
    if (m_camera) {
        const QnResourceVideoLayout* layout = m_camera->getVideoLayout();

        for (int i = 0; i < layout->numberOfChannels(); ++i) {
            QRect r(0, 0, MD_WIDTH, MD_HEIGHT);
            r.translate(layout->h_position(i)*MD_WIDTH, layout->v_position(i)*MD_HEIGHT);
            r = gridRect.intersected(r);
            r.translate(-layout->h_position(i)*MD_WIDTH, -layout->v_position(i)*MD_HEIGHT);
            if (!r.isEmpty()) {
                m_motionSensitivity[i].addRect(sensitivity, r);
                changed = true;
            }
        }
    }

    if(sensitivity == 0)
        invalidateBinaryMotionMask();

    return changed;
}

bool QnMediaResourceWidget::setMotionSensitivityFilled(const QPoint &gridPos, int sensitivity) {
    ensureMotionSensitivity();

    int channel =0;
    QPoint channelPos = gridPos;
    if (m_camera) {
        const QnResourceVideoLayout* layout = m_camera->getVideoLayout();

        for (int i = 0; i < layout->numberOfChannels(); ++i) {
            QRect r(layout->h_position(i) * MD_WIDTH, layout->v_position(i) * MD_HEIGHT, MD_WIDTH, MD_HEIGHT);
            if (r.contains(channelPos)) {
                channelPos -= r.topLeft();
                channel = i;
                break;
            }
        }
    }
    return m_motionSensitivity[channel].updateSensitivityAt(channelPos, sensitivity);
}

void QnMediaResourceWidget::clearMotionSensitivity() {
    for(int i = 0; i < channelCount(); i++)
        m_motionSensitivity[i] = QnMotionRegion();
    m_motionSensitivityValid = true;

    invalidateBinaryMotionMask();
}

const QList<QnMotionRegion> &QnMediaResourceWidget::motionSensitivity() const {
    ensureMotionSensitivity();

    return m_motionSensitivity;
}

void QnMediaResourceWidget::ensureBinaryMotionMask() const {
    if(m_binaryMotionMaskValid)
        return;

    ensureMotionSensitivity();
    for (int i = 0; i < channelCount(); ++i)
        QnMetaDataV1::createMask(m_motionSensitivity[i].getMotionMask(), reinterpret_cast<char *>(m_binaryMotionMask[i]));
}

void QnMediaResourceWidget::invalidateBinaryMotionMask() {
    m_binaryMotionMaskValid = false;
}

void QnMediaResourceWidget::ensureMotionSelectionCache() {
    if (m_motionSelectionCacheValid)
        return;

    for (int i = 0; i < channelCount(); ++i){
        QPainterPath path;
        path.addRegion(m_motionSelection[i]);
        m_motionSelectionPathCache[i] = path.simplified();
    }
}

void QnMediaResourceWidget::invalidateMotionSelectionCache() {
    m_motionSelectionCacheValid = false;
}

Qn::ResolutionMode QnMediaResourceWidget::resolutionMode() const {
    return m_resolutionMode;
}

void QnMediaResourceWidget::setResolutionMode(Qn::ResolutionMode resolutionMode) {
    if(resolutionMode < 0 || resolutionMode >= Qn::ResolutionModeCount) {
        qnWarning("Invalid resolution mode '%1'.", static_cast<int>(resolutionMode));
        return;
    }

    if(m_resolutionMode == resolutionMode)
        return;

    m_resolutionMode = resolutionMode;

    // TODO: #VASILENKO insert code here

    updateRadassButton();

    emit resolutionModeChanged();
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnMediaResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);

    if(isDecorationsVisible() && isInfoVisible())
        updateInfoTextLater();
}

Qn::RenderStatus QnMediaResourceWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &rect) {
    painter->beginNativePainting();

    qreal opacity = effectiveOpacity();
    bool opaque = qFuzzyCompare(opacity, 1.0);
    if(!opaque) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    Qn::RenderStatus result = m_renderer->paint(channel, rect, effectiveOpacity());
    
    /* There is no need to restore blending state before invoking endNativePainting. */
    painter->endNativePainting();

    if(result != Qn::NewFrameRendered && result != Qn::OldFrameRendered)
        painter->fillRect(rect, Qt::black);

    return result;
}

void QnMediaResourceWidget::paintChannelForeground(QPainter *painter, int channel, const QRectF &rect) {
    if (options() & DisplayMotion) {
        ensureMotionSelectionCache();

        paintMotionGrid(painter, channel, rect, m_renderer->lastFrameMetadata(channel));
        paintMotionSensitivity(painter, channel, rect);

        /* Motion selection. */
        if(!m_motionSelection[channel].isEmpty()) {
            QColor color = toTransparent(qnGlobals->mrsColor(), 0.2);
            paintFilledRegionPath(painter, rect, m_motionSelectionPathCache[channel], color, color);
        }
    }
}

void QnMediaResourceWidget::paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion) {
    // 5-7 fps

    ensureMotionSensitivity();

    qreal xStep = rect.width() / MD_WIDTH;
    qreal yStep = rect.height() / MD_HEIGHT;

    QVector<QPointF> gridLines[2];

    if (motion && motion->channelNumber == (quint32)channel) {
        // 2-3 fps

        ensureBinaryMotionMask();
        motion->removeMotion(m_binaryMotionMask[channel]);

        /* Horizontal lines. */
        for (int y = 1; y < MD_HEIGHT; ++y) {
            bool isMotion = motion->isMotionAt(0, y - 1) || motion->isMotionAt(0, y);
            gridLines[isMotion] << QPointF(0, y * yStep);
            int x = 1;
            while(x < MD_WIDTH){
                while (x < MD_WIDTH && isMotion == (motion->isMotionAt(x, y - 1) || motion->isMotionAt(x, y)) )
                    x++;
                gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                if (x < MD_WIDTH){
                    isMotion = !isMotion;
                    gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                }
            }
        }

        /* Vertical lines. */
        for (int x = 1; x < MD_WIDTH; ++x) {
            bool isMotion = motion->isMotionAt(x - 1, 0) || motion->isMotionAt(x, 0);
            gridLines[isMotion] << QPointF(x * xStep, 0);
            int y = 1;
            while(y < MD_HEIGHT){
                while (y < MD_HEIGHT && isMotion == (motion->isMotionAt(x - 1, y) || motion->isMotionAt(x, y)) )
                    y++;
                gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                if (y < MD_HEIGHT){
                    isMotion = !isMotion;
                    gridLines[isMotion] << QPointF(x * xStep, y * yStep);
                }
            }
        }
    } else {
        for (int x = 1; x < MD_WIDTH; ++x)
            gridLines[0] << QPointF(x * xStep, 0.0) << QPointF(x * xStep, rect.height());
        for (int y = 1; y < MD_HEIGHT; ++y)
            gridLines[0] << QPointF(0.0, y * yStep) << QPointF(rect.width(), y * yStep);
    }


    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(rect.topLeft());

    QnScopedPainterPenRollback penRollback(painter);
    painter->setPen(QPen(QColor(255, 255, 255, 16)));
    painter->drawLines(gridLines[0]);

    painter->setPen(QPen(QColor(255, 0, 0, 128)));
    painter->drawLines(gridLines[1]);
}

void QnMediaResourceWidget::paintFilledRegionPath(QPainter *painter, const QRectF &rect, const QPainterPath &path, const QColor &color, const QColor &penColor) {
    // 4-6 fps

    QnScopedPainterTransformRollback transformRollback(painter); Q_UNUSED(transformRollback)

    QnScopedPainterBrushRollback brushRollback(painter, color); Q_UNUSED(brushRollback)

    painter->translate(rect.topLeft());
    painter->scale(rect.width() / MD_WIDTH, rect.height() / MD_HEIGHT);
    painter->setPen(QPen(penColor));
    painter->drawPath(path);
}

void QnMediaResourceWidget::paintMotionSensitivityIndicators(QPainter *painter, int channel, const QRectF &rect, const QnMotionRegion &region) {
    Q_UNUSED(channel)
    qreal xStep = rect.width() / MD_WIDTH;
    qreal yStep = rect.height() / MD_HEIGHT;

    painter->setPen(Qt::black);
    QFont font;
    font.setPointSizeF(0.03 * rect.width());
    painter->setFont(font);

    for (int sensitivity = QnMotionRegion::MIN_SENSITIVITY + 1; sensitivity <= QnMotionRegion::MAX_SENSITIVITY; ++sensitivity) {
        foreach(const QRect &rect, region.getRectsBySens(sensitivity)) {
            if (rect.width() < 2 || rect.height() < 2)
                continue;

            int x = rect.left(), y = rect.top();
            painter->drawStaticText(x * xStep + xStep * 0.1, y * yStep + yStep * 0.1, m_sensStaticText[sensitivity]);
        }
    }
}

void QnMediaResourceWidget::paintMotionSensitivity(QPainter *painter, int channel, const QRectF &rect) {
    ensureMotionSensitivity();

    if (options() & DisplayMotionSensitivity) {
        for (int i = QnMotionRegion::MIN_SENSITIVITY; i <= QnMotionRegion::MAX_SENSITIVITY; ++i) {
            QColor color = i > 0 ? QColor(100 +  i * 3, 16 * (10 - i), 0, 96 + i * 2) : qnGlobals->motionMaskColor();
            QPainterPath path = m_motionSensitivity[channel].getRegionBySensPath(i);
            paintFilledRegionPath(painter, rect, path, color, Qt::black);
        }

        paintMotionSensitivityIndicators(painter, channel, rect, m_motionSensitivity[channel]);
    } else {
        paintFilledRegionPath(painter, rect, m_motionSensitivity[channel].getMotionMaskPath(), qnGlobals->motionMaskColor(), qnGlobals->motionMaskColor());
    }
}

void QnMediaResourceWidget::updateIconButton() {
    if(!m_camera) {
        iconButton()->setVisible(false);
        return;
    }

    int recordingMode = Qn::RecordingType_Never;
    if(m_camera->getStatus() == QnResource::Recording)
        recordingMode = currentRecordingMode();
    
    iconButton()->setVisible(true);
    switch(recordingMode) {
    case Qn::RecordingType_Never:
        iconButton()->setIcon(qnSkin->icon("item/recording_off.png"));
        iconButton()->setToolTip(tr("Not recording."));
        break;
    case Qn::RecordingType_Run:
        iconButton()->setIcon(qnSkin->icon("item/recording.png"));
        iconButton()->setToolTip(tr("Recording everything."));
        break;
    case Qn::RecordingType_MotionOnly:
        iconButton()->setIcon(qnSkin->icon("item/recording_motion.png"));
        iconButton()->setToolTip(tr("Recording motion only."));
        break;
    case Qn::RecordingType_MotionPlusLQ:
        iconButton()->setIcon(qnSkin->icon("item/recording_motion_lq.png"));
        iconButton()->setToolTip(tr("Recording motion and low quality."));
        break;
    default:
        iconButton()->setVisible(false);
        break;
    }
}

void QnMediaResourceWidget::updateRadassButton() {
    QString iconPath;
    switch(m_resolutionMode) {
    case Qn::AutoResolution:
        iconPath = QLatin1String("item/radass_auto.png");
        break;
    case Qn::LowResolution:
        iconPath = QLatin1String("item/radass_low.png");
        break;
    case Qn::HighResolution:
        iconPath = QLatin1String("item/radass_high.png");
        break;
    default:
        assert(false);
    }

    buttonBar()->button(RadassButton)->setIcon(qnSkin->icon(iconPath));
}

int QnMediaResourceWidget::currentRecordingMode() {
    if(!m_camera)
        return Qn::RecordingType_Never;

    // TODO: this should be a resource parameter that is update from the server.

    QDateTime dateTime = qnSyncTime->currentDateTime().addMSecs(context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(m_resource, 0));
    int dayOfWeek = dateTime.date().dayOfWeek();
    int seconds = QTime().secsTo(dateTime.time());

    foreach(const QnScheduleTask &task, m_camera->getScheduleTasks())
        if(task.getDayOfWeek() == dayOfWeek && task.getStartTime() <= seconds && seconds <= task.getEndTime())
            return task.getRecordingType();

    return Qn::RecordingType_Never;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
Qn::WindowFrameSections QnMediaResourceWidget::windowFrameSectionsAt(const QRectF &region) const {
    if(options() & ControlPtz) {
        return Qn::NoSection; /* No resizing when PTZ control is ON. */
    } else {
        return base_type::windowFrameSectionsAt(region);
    }
}

int QnMediaResourceWidget::helpTopicAt(const QPointF &pos) const {
    Q_UNUSED(pos)
    if(options() & DisplayMotionSensitivity) {
        return Qn::CameraSettings_Motion_Help;
    } else if(options() & DisplayMotion) {
        return Qn::MainWindow_MediaItem_SmartSearch_Help;
    } else if(m_resource->flags() & QnResource::local) {
        return Qn::MainWindow_MediaItem_Local_Help;
    } else {
        return Qn::MainWindow_MediaItem_Help;
    }
}

void QnMediaResourceWidget::channelLayoutChangedNotify() {
    base_type::channelLayoutChangedNotify();

    resizeList(m_motionSelection, channelCount());
    resizeList(m_motionSelectionPathCache, channelCount());
    resizeList(m_motionSensitivity, channelCount());

    while(m_binaryMotionMask.size() > channelCount()) {
        qFreeAligned(m_binaryMotionMask.back());
        m_binaryMotionMask.pop_back();
    }
    while(m_binaryMotionMask.size() < channelCount()) {
        m_binaryMotionMask.push_back(static_cast<__m128i *>(qMallocAligned(MD_WIDTH * MD_HEIGHT / 8, 32)));
        memset(m_binaryMotionMask.back(), 0, MD_WIDTH * MD_HEIGHT / 8);
    }
}

void QnMediaResourceWidget::channelScreenSizeChangedNotify() {
    base_type::channelScreenSizeChangedNotify();

    m_renderer->setChannelScreenSize(channelScreenSize());
}

void QnMediaResourceWidget::optionsChangedNotify(Options changedFlags) {
    if(changedFlags & DisplayMotion) {
        if (QnAbstractArchiveReader *reader = m_display->archiveReader())
            reader->setSendMotion(options() & DisplayMotion);

        buttonBar()->setButtonsChecked(MotionSearchButton, options() & DisplayMotion);

        if(options() & DisplayMotion) {
            setProperty(Qn::MotionSelectionModifiers, 0);
        } else {
            setProperty(Qn::MotionSelectionModifiers, QVariant()); /* Use defaults. */
        }
    }
    base_type::optionsChangedNotify(changedFlags);
}

QString QnMediaResourceWidget::calculateInfoText() const {
    qreal fps = 0.0;
    qreal mbps = 0.0;
    for(int i = 0; i < channelCount(); i++) {
        const QnStatistics *statistics = m_display->mediaProvider()->getStatistics(i);
        fps = qMax(fps, static_cast<qreal>(statistics->getFrameRate()));
        mbps += statistics->getBitrate();
    }

    QSize size = m_display->camDisplay()->getFrameSize(0);
    if(size.isEmpty())
        size = QSize(0, 0);

    QString codecString;
    if(QnMediaContextPtr codecContext = m_display->mediaProvider()->getCodecContext()) {
        codecString = codecContext->codecName();
        if(!codecString.isEmpty())
            codecString = tr(" (%1)").arg(codecString);
    }

    QString hqLqString;
#ifdef QN_MEDIA_RESOURCE_WIDGET_SHOW_HI_LO_RES
    hqLqString = (m_renderer->isLowQualityImage(0)) ? tr(" Lo-Res") : tr(" Hi-Res");
#endif

    QString timeString;
    if (m_resource->flags() & QnResource::utc) { /* Do not show time for regular media files. */
        qint64 utcTime = m_renderer->lastDisplayedTime(0) / 1000;
        if(qnSettings->timeMode() == Qn::ServerTimeMode)
            utcTime += context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(m_resource, 0); // TODO: do offset adjustments in one place

        timeString = tr("\t%1").arg(
            m_display->camDisplay()->isRealTimeSource() ? 
            tr("LIVE") :
            QDateTime::fromMSecsSinceEpoch(utcTime).toString(lit("hh:mm:ss.zzz"))
        );
    }

    // TODO: #AK either remove these commented code blocks, leave a TODO, 
    // or use #ifdef based on some other define
//#ifdef _DEBUG
    QString decoderType = m_renderer->isHardwareDecoderUsed(0) ? tr(" HW") : tr(" SW");
//#endif

    return tr("%1x%2 %3fps @ %4Mbps%5%6%7%8").arg(size.width()).arg(size.height()).arg(fps, 0, 'f', 2).arg(mbps, 0, 'f', 2)
        .arg(codecString).arg(hqLqString)
//#ifdef _DEBUG
        .arg(decoderType)
//#else
//        .arg(QString())
//#endif
        .arg(timeString);
}

QnResourceWidget::Buttons QnMediaResourceWidget::calculateButtonsVisibility() const {
    Buttons result = base_type::calculateButtonsVisibility() & ~InfoButton;

    if(!(resource()->flags() & QnResource::still_image))
        result |= InfoButton;

    if (resource()->flags() & QnResource::motion)
        result |= MotionSearchButton | InfoButton;

    if(m_camera) {
        if(
            (m_camera->getCameraCapabilities() & (Qn::ContinuousPanTiltCapability | Qn::ContinuousZoomCapability)) && 
            accessController()->hasPermissions(m_resource, Qn::WritePtzPermission) 
        ) {
            result |= PtzButton;
        }

        result |= RadassButton;
    }

    return result;
}

QnResourceWidget::Overlay QnMediaResourceWidget::calculateChannelOverlay(int channel) const {
    if (m_display->camDisplay()->isStillImage()) {
        return EmptyOverlay;
    } else if (m_display->isPaused() && (options() & DisplayActivityOverlay)) {
        return PausedOverlay;
    } else if (m_display->camDisplay()->isRealTimeSource() && m_display->resource()->getStatus() == QnResource::Offline) {
        return OfflineOverlay;
    } else if (m_display->camDisplay()->isRealTimeSource() && m_display->resource()->getStatus() == QnResource::Unauthorized) {
        return UnauthorizedOverlay;
    } else if (m_display->camDisplay()->isLongWaiting()) {
        if (m_display->camDisplay()->isEOFReached())
            return NoDataOverlay;
        QnCachingTimePeriodLoader *loader = context()->navigator()->loader(m_resource);
        if (loader && loader->periods(Qn::RecordingRole).containTime(m_display->camDisplay()->getExternalTime() / 1000))
            return base_type::calculateChannelOverlay(channel, QnResource::Online);
        else
            return NoDataOverlay;
    } else if (m_display->isPaused()) {
        return EmptyOverlay;
    } else {
        return base_type::calculateChannelOverlay(channel, QnResource::Online);
    }
}

void QnMediaResourceWidget::at_resource_resourceChanged() {
    invalidateMotionSensitivity();
}

void QnMediaResourceWidget::at_renderer_sourceSizeChanged(const QSize &size) {
    setAspectRatio(static_cast<qreal>(size.width() * channelLayout()->width()) / (size.height() * channelLayout()->height()));
}

void QnMediaResourceWidget::at_camDisplay_liveChanged() {
    bool isLive = m_display->camDisplay()->isRealTimeSource();

    if(!isLive)
        buttonBar()->setButtonsChecked(PtzButton, false);
}

void QnMediaResourceWidget::at_searchButton_toggled(bool checked) {
    setOption(DisplayMotion, checked);

    if(checked)
        buttonBar()->setButtonsChecked(PtzButton, false);
}

void QnMediaResourceWidget::at_ptzButton_toggled(bool checked) {
    bool ptzEnabled = checked && (m_camera->getCameraCapabilities() & Qn::ContinuousPanTiltCapability | Qn::ContinuousZoomCapability);

    setOption(ControlPtz, ptzEnabled);
    setOption(DisplayCrosshair, ptzEnabled);

    if(checked) {
        buttonBar()->setButtonsChecked(MotionSearchButton, false);
        action(Qn::JumpToLiveAction)->trigger(); // TODO: evil hack! Won't work if SYNC is off and this item is not selected?
    }
}

void QnMediaResourceWidget::at_radassButton_clicked() {
    setResolutionMode(static_cast<Qn::ResolutionMode>((resolutionMode() + 1) % Qn::ResolutionModeCount));
}

void QnMediaResourceWidget::at_replyReceived(int status, int handle) {
    Q_UNUSED(status);
    Q_UNUSED(handle);
}

void QnMediaResourceWidget::at_replyReceived(int status, const QList<QPair<QString, bool> > &operationResult) {
    Q_UNUSED(status);
    Q_UNUSED(operationResult);
}





