#include "media_resource_widget.h"

#include <QtGui/QPainter>

#include <plugins/resources/archive/abstract_archive_stream_reader.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/media_resource.h>
#include <core/resource/security_cam_resource.h>

#include <camera/resource_display.h>
#include <camera/camdisplay.h>

#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/common/color_transformations.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

#include "resource_widget_renderer.h"
#include "image_button_widget.h"
#include "image_button_bar.h"
#include "resource_widget.h"


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
    QnResourceWidget(context, item, parent)
{
    m_resource = base_type::resource().dynamicCast<QnMediaResource>();
    if(!m_resource) 
        qnCritical("Media resource widget was created with a non-media resource.");

    /* Set up video rendering. */
    m_display = new QnResourceDisplay(m_resource, this);
    connect(m_resource.data(), SIGNAL(resourceChanged()), this, SLOT(at_resource_resourceChanged()));
    connect(m_display->camDisplay(), SIGNAL(stillImageChanged()), this, SLOT(updateButtonsVisibility()));
    setChannelLayout(m_display->videoLayout());

    m_renderer = new QnResourceWidgetRenderer(channelCount());
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
    m_searchButton = new QnImageButtonWidget();
    m_searchButton->setIcon(qnSkin->icon("decorations/item_search.png"));
    m_searchButton->setCheckable(true);
    m_searchButton->setProperty(Qn::NoBlockMotionSelection, true);
    connect(m_searchButton, SIGNAL(toggled(bool)), this, SLOT(at_searchButton_toggled(bool)));

    buttonBar()->addButton(MotionSearchButton, m_searchButton);
    updateButtonsVisibility();
}

QnMediaResourceWidget::~QnMediaResourceWidget() {
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

    if(changed)
        emit motionSelectionChanged();
}

void QnMediaResourceWidget::clearMotionSelection() {
    if(isMotionSelectionEmpty())
        return;

    for (int i = 0; i < m_motionSelection.size(); ++i)
        m_motionSelection[i] = QRegion();

    emit motionSelectionChanged();
}

void QnMediaResourceWidget::invalidateMotionSensitivity() {
    m_motionSensitivityValid = false;
}

void QnMediaResourceWidget::ensureMotionSensitivity() const {
    if(m_motionSensitivityValid)
        return;

    if (QnSecurityCamResourcePtr camera = m_resource.dynamicCast<QnSecurityCamResource>()) {
        m_motionSensitivity = camera->getMotionRegionList();

        if(m_motionSensitivity.size() != channelCount()) {
            qnWarning("Camera '%1' returned a motion sensitivity list of invalid size.", camera->getName());
            resizeList(m_motionSensitivity, channelCount());
        }
    } else {
        m_motionSensitivity.clear();
    }

    m_motionSensitivityValid = true;
}

bool QnMediaResourceWidget::addToMotionSensitivity(const QRect &gridRect, int sensitivity) {
    ensureMotionSensitivity();

    bool changed = false;
    if (QnSecurityCamResourcePtr camera = m_resource.dynamicCast<QnSecurityCamResource>()) {
        const QnVideoResourceLayout* layout = camera->getVideoLayout();

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
    if (QnSecurityCamResourcePtr camera = m_resource.dynamicCast<QnSecurityCamResource>()) {
        const QnVideoResourceLayout* layout = camera->getVideoLayout();

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



// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnMediaResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);

    if(isDecorationsVisible() && isInfoVisible())
        updateInfoTextLater();
}

Qn::RenderStatus QnMediaResourceWidget::paintChannel(QPainter *painter, int channel, const QRectF &rect) {
    painter->beginNativePainting();
    Qn::RenderStatus result = m_renderer->paint(channel, rect, effectiveOpacity());
    painter->endNativePainting();

    if(result != Qn::NewFrameRendered && result != Qn::OldFrameRendered)
        painter->fillRect(rect, Qt::black);

    /* Draw motion grid. */
    if (displayFlags() & DisplayMotion) {
        for(int i = 0; i < channelCount(); i++)  {
            QRectF rect = channelRect(i);

            paintMotionGrid(painter, i, rect, m_renderer->lastFrameMetadata(i));
            paintMotionSensitivity(painter, i, rect);

            /* Selection. */
            if(!m_motionSelection[i].isEmpty()) {
                QColor color = toTransparent(qnGlobals->mrsColor(), 0.2);
                paintFilledRegion(painter, rect, m_motionSelection[i], color, color);
            }
        }
    }

    return result;
}

void QnMediaResourceWidget::paintMotionGrid(QPainter *painter, int channel, const QRectF &rect, const QnMetaDataV1Ptr &motion) {
    ensureMotionSensitivity();

    qreal xStep = rect.width() / MD_WIDTH;
    qreal yStep = rect.height() / MD_HEIGHT;

    QVector<QPointF> gridLines;

    for (int x = 0; x < MD_WIDTH; ++x) {
        if (m_motionSensitivity[channel].isEmpty()) {
            gridLines << QPointF(x * xStep, 0.0) << QPointF(x * xStep, rect.height());
        } else {
            QRegion lineRect(x, 0, 1, MD_HEIGHT + 1);
            QRegion drawRegion = lineRect - m_motionSensitivity[channel].getMotionMask().intersect(lineRect);
            foreach(const QRect& r, drawRegion.rects())
                gridLines << QPointF(x * xStep, r.top() * yStep) << QPointF(x * xStep, qMin(rect.height(), (r.top() + r.height()) * yStep));
        }
    }

    for (int y = 0; y < MD_HEIGHT; ++y) {
        if (m_motionSensitivity[channel].isEmpty()) {
            gridLines << QPointF(0.0, y * yStep) << QPointF(rect.width(), y * yStep);
        } else {
            QRegion lineRect(0, y, MD_WIDTH + 1, 1);
            QRegion drawRegion = lineRect - m_motionSensitivity[channel].getMotionMask().intersect(lineRect);
            foreach(const QRect& r, drawRegion.rects())
                gridLines << QPointF(r.left() * xStep, y * yStep) << QPointF(qMin(rect.width(), (r.left() + r.width()) * xStep), y * yStep);
        }
    }

    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(rect.topLeft());

    QnScopedPainterPenRollback penRollback(painter);
    painter->setPen(QPen(QColor(255, 255, 255, 16)));
    painter->drawLines(gridLines);

    if (motion && motion->channelNumber == channel) {
        ensureBinaryMotionMask();

        QPainterPath motionPath;
        motion->removeMotion(m_binaryMotionMask[channel]);
        for (int y = 0; y < MD_HEIGHT; ++y)
            for (int x = 0; x < MD_WIDTH; ++x)
                if(motion->isMotionAt(x, y))
                    motionPath.addRect(QRectF(QPointF(x * xStep, y * yStep), QPointF((x + 1) * xStep, (y + 1) * yStep)));

        painter->setPen(QPen(QColor(255, 0, 0, 128)));
        painter->drawPath(motionPath);
    }
}

void QnMediaResourceWidget::paintFilledRegion(QPainter *painter, const QRectF &rect, const QRegion &selection, const QColor &color, const QColor &penColor) {
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

void QnMediaResourceWidget::paintMotionSensitivityIndicators(QPainter *painter, int channel, const QRectF &rect, const QnMotionRegion &region) {
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

    if (displayFlags() & DisplayMotionSensitivity) {
        for (int i = 0; i < 10; ++i) {
            QColor color = i > 0 ? QColor(100 +  i * 3, 16 * (10 - i), 0, 96 + i * 2) : qnGlobals->motionMaskColor();
            QRegion region = m_motionSensitivity[channel].getRegionBySens(i);
            if (i > 0)
                region -= m_motionSensitivity[channel].getRegionBySens(0);
            paintFilledRegion(painter, rect, region, color, Qt::black);
        }

        paintMotionSensitivityIndicators(painter, channel, rect, m_motionSensitivity[channel]);
    } else {
        paintFilledRegion(painter, rect, m_motionSensitivity[channel].getMotionMask(), qnGlobals->motionMaskColor(), qnGlobals->motionMaskColor());
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnMediaResourceWidget::channelLayoutChangedNotify() {
    base_type::channelLayoutChangedNotify();

    resizeList(m_motionSelection, channelCount());
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

void QnMediaResourceWidget::displayFlagsChangedNotify(DisplayFlags changedFlags) {
    if(changedFlags & DisplayMotion) {
        if (QnAbstractArchiveReader *reader = m_display->archiveReader())
            reader->setSendMotion(displayFlags() & DisplayMotion);

        m_searchButton->setChecked(displayFlags() & DisplayMotion);

        if(displayFlags() & DisplayMotion) {
            setProperty(Qn::MotionSelectionModifiers, 0);
        } else {
            setProperty(Qn::MotionSelectionModifiers, QVariant()); /* Use defaults. */
        }
    }
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

    QString codecName;
    if(QnMediaContextPtr codecContext = m_display->mediaProvider()->getCodecContext())
        codecName = codecContext->codecName();
    
    QString result = tr("%1x%2 %3fps @ %4Mbps (%5)").arg(size.width()).arg(size.height()).arg(fps, 0, 'f', 2).arg(mbps, 0, 'f', 2).arg(codecName);

    if (m_resource->flags() & QnResource::utc) {
        /* Do not show time for regular media files. */
        result.
            append(QLatin1Char('\t')).
            append(QDateTime::fromMSecsSinceEpoch(m_renderer->lastDisplayedTime(0) / 1000).toString(tr("hh:mm:ss.zzz")));
    }

    return result;
}

QnResourceWidget::Buttons QnMediaResourceWidget::calculateButtonsVisibility() const {
    Buttons result = base_type::calculateButtonsVisibility() & ~InfoButton;

    if(!display()->camDisplay()->isStillImage())
        result |= InfoButton;

    if(m_resource.dynamicCast<QnSecurityCamResource>())
        result |= MotionSearchButton;

    return result;
}

QnResourceWidget::Overlay QnMediaResourceWidget::calculateChannelOverlay(int channel) const {
    if (m_display->camDisplay()->isStillImage()) {
        return EmptyOverlay;
    } else if(m_display->isPaused() && (displayFlags() & DisplayActivityOverlay)) {
        return PausedOverlay;
    } else if (m_display->camDisplay()->isRealTimeSource() && m_display->resource()->getStatus() == QnResource::Offline) {
        return OfflineOverlay;
    } else if (m_display->camDisplay()->isRealTimeSource() && m_display->resource()->getStatus() == QnResource::Unauthorized) {
        return UnauthorizedOverlay;
    } else if (m_display->camDisplay()->isNoData()) {
        return NoDataOverlay;
    } else if(m_display->isPaused()) {
        return EmptyOverlay;
    } else {
        return base_type::calculateChannelOverlay(channel);
    }
}

void QnMediaResourceWidget::at_resource_resourceChanged() {
    invalidateMotionSensitivity();
}

void QnMediaResourceWidget::at_renderer_sourceSizeChanged(const QSize &size) {
    setAspectRatio(static_cast<qreal>(size.width() * channelLayout()->width()) / (size.height() * channelLayout()->height()));
}

void QnMediaResourceWidget::at_searchButton_toggled(bool checked) {
    setDisplayFlag(DisplayMotion, checked);
}
