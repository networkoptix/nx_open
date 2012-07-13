#include "media_resource_widget.h"

#include <camera/resource_display.h>

#include "resource_widget_renderer.h"


QnMediaResourceWidget::QnMediaResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    QnResourceWidget(context, item, parent)
{
    
}

QnMediaResourceWidget::~QnMediaResourceWidget() {

    delete m_display;

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        qFreeAligned(m_motionMaskBinData[i]);

}

QnResourcePtr QnMediaResourceWidget::resource() const {
    return m_display->resource();
}

int QnMediaResourceWidget::motionGridWidth() const
{
    return MD_WIDTH * m_contentLayout->width();
}

int QnMediaResourceWidget::motionGridHeight() const
{
    return MD_HEIGHT * m_contentLayout->height();
}

QPoint QnMediaResourceWidget::mapToMotionGrid(const QPointF &itemPos)
{
    QPointF gridPosF(cwiseDiv(itemPos, toPoint(cwiseDiv(size(), QSizeF(motionGridWidth(), motionGridHeight())))));
    QPoint gridPos(qFuzzyFloor(gridPosF.x()), qFuzzyFloor(gridPosF.y()));

    return bounded(gridPos, QRect(0, 0, motionGridWidth(), motionGridHeight()));
}

QPointF QnMediaResourceWidget::mapFromMotionGrid(const QPoint &gridPos) {
    return cwiseMul(gridPos, toPoint(cwiseDiv(size(), QSizeF(motionGridWidth(), motionGridHeight()))));
}

void QnMediaResourceWidget::addToMotionSelection(const QRect &gridRect) 
{
    QList<QRegion> prevSelection;
    QList<QRegion> newSelection;

    ensureMotionMask();

    for (int i = 0; i < m_channelState.size(); ++i)
    {
        prevSelection << m_channelState[i].motionSelection;

        QRect r(0, 0, MD_WIDTH, MD_HEIGHT);
        r.translate(m_contentLayout->h_position(i)*MD_WIDTH, m_contentLayout->v_position(i)*MD_HEIGHT);
        r = gridRect.intersected(r);

        if (r.width() > 0 && r.height() > 0) 
        {
            QRegion r1(r);
            r1.translate(-m_contentLayout->h_position(i)*MD_WIDTH, -m_contentLayout->v_position(i)*MD_HEIGHT);
            r1 -= m_motionRegionList[i].getMotionMask();

            m_channelState[i].motionSelection += r1;
        }

        newSelection << m_channelState[i].motionSelection;
    }

    if(prevSelection != newSelection)
        emit motionSelectionChanged();
}

void QnMediaResourceWidget::clearMotionSelection() 
{
    bool allEmpty = true;
    foreach(const ChannelState &state, m_channelState)
        allEmpty &= state.motionSelection.isEmpty();
    if (allEmpty)
        return;

    for (int i = 0; i < m_channelState.size(); ++i)
        m_channelState[i].motionSelection = QRegion();

    emit motionSelectionChanged();
}

QList<QRegion> QnMediaResourceWidget::motionSelection() const {
    QList<QRegion> result;
    foreach(const ChannelState &state, m_channelState)
        result.push_back(state.motionSelection);
    return result;
}

void QnMediaResourceWidget::invalidateMotionMask() {
    m_motionMaskValid = false;
}

void QnMediaResourceWidget::addToMotionRegion(int sensitivity, const QRect& rect, int channel) {
    ensureMotionMask();

    m_motionRegionList[channel].addRect(sensitivity, rect);

    /*
    QnMotionRegion newRegion = m_motionRegionList[channel];
    newRegion.addRect(sens, rect);
    if (newRegion.isValid()) 
    {
        m_motionRegionList[channel] = newRegion;
        invalidateMotionMaskBinData();
    }
    return newRegion.isValid();
    */
}

bool QnMediaResourceWidget::setMotionRegionSensitivity(int sensitivity, const QPoint &gridPos, int channel) {
    ensureMotionMask();

    return m_motionRegionList[channel].updateSensitivityAt(gridPos, sensitivity);
}

void QnMediaResourceWidget::clearMotionRegions() {
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionRegionList[i] = QnMotionRegion();
    m_motionMaskValid = true;

    invalidateMotionMaskBinData();
}

void QnMediaResourceWidget::ensureMotionMask()
{
    if(m_motionMaskValid)
        return;

    if (QnSecurityCamResourcePtr camera = m_resource.dynamicCast<QnSecurityCamResource>()) {
        m_motionRegionList = camera->getMotionRegionList();

        while(m_motionRegionList.size() < CL_MAX_CHANNELS)
            m_motionRegionList.push_back(QnMotionRegion()); /* Just to feel safe. */
    }
    m_motionMaskValid = true;
}

void QnMediaResourceWidget::ensureMotionMaskBinData() {
    if(m_motionMaskBinDataValid)
        return;

    ensureMotionMask();
    Q_ASSERT(((unsigned long)m_motionMaskBinData[0])%16 == 0);
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        QnMetaDataV1::createMask(m_motionRegionList[i].getMotionMask(), (char*)m_motionMaskBinData[i]);
}

void QnMediaResourceWidget::invalidateMotionMaskBinData() {
    m_motionMaskBinDataValid = false;
}





void QnMediaResourceWidget::updateOverlayText() {
    qint64 time = m_renderer->lastDisplayedTime(0);
    if (time > 1000000ll * 3600 * 24) /* Do not show time for regular media files. */ // TODO: try to avoid time comparison hacks.
        m_footerRightLabel->setText(QDateTime::fromMSecsSinceEpoch(time/1000).toString(tr("hh:mm:ss.zzz")));

    qreal fps = 0.0;
    qreal mbps = 0.0;
    for(int i = 0; i < m_channelCount; i++) {
        const QnStatistics *statistics = m_display->mediaProvider()->getStatistics(i);
        fps = qMax(fps, static_cast<qreal>(statistics->getFrameRate()));
        mbps += statistics->getBitrate();
    }

    QSize size = m_display->camDisplay()->getFrameSize(0); // TODO: roma, check implementation.
    if(size.isEmpty())
        size = QSize(0, 0);

    QString codecName;
    QnMediaContextPtr codec = m_display->mediaProvider()->getCodecContext();
    if (codec && codec->ctx())
        codecName = codecIDToString(codec->ctx()->codec_id);
    m_footerLeftLabel->setText(tr("%1x%2 %3fps @ %4Mbps (%5)").arg(size.width()).arg(size.height()).arg(fps, 0, 'f', 2).arg(mbps, 0, 'f', 2).arg(codecName));
}


void QnMediaResourceWidget::at_resource_resourceChanged() {
    invalidateMotionMask();
}


void QnMediaResourceWidget::channelScreenSizeChangedNotify() {
    base_type::channelScreenSizeChangedNotify();

    m_renderer->setChannelScreenSize(channelScreenSize());
}

Qn::RenderStatus QnMediaResourceWidget::paintChannel(QPainter *painter, int channel, const QRectF &rect) {
    painter->beginNativePainting();
    Qn::RenderStatus result = m_renderer->paint(channel, rect, effectiveOpacity());
    painter->endNativePainting();

    /* Draw motion grid. */
    if (m_displayFlags & DisplayMotion) {
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


    return result;
}


void QnResourceWidget::drawMotionGrid(QPainter *painter, const QRectF& rect, const QnMetaDataV1Ptr &motion, int channel) {
    qreal xStep = rect.width() / MD_WIDTH;
    qreal yStep = rect.height() / MD_HEIGHT;

    ensureMotionMask();

#if 1
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
    painter->setPen(QPen(QColor(255, 255, 255, 16)));
    painter->translate(rect.topLeft());
    painter->drawLines(gridLines);
#endif

    if (!motion || motion->channelNumber != channel)
        return;

    ensureMotionMaskBinData();

    QPainterPath motionPath;
    motion->removeMotion(m_motionMaskBinData[motion->channelNumber]);
    for (int y = 0; y < MD_HEIGHT; ++y)
        for (int x = 0; x < MD_WIDTH; ++x)
            if(motion->isMotionAt(x, y))
                motionPath.addRect(QRectF(QPointF(x * xStep, y * yStep), QPointF((x + 1) * xStep, (y + 1) * yStep)));
    QPen pen(QColor(255, 0, 0, 128));
    qreal unit = qnGlobals->workbenchUnitSize();
    pen.setWidth(unit * 0.003);
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
        foreach(const QRect& rect, region.getRectsBySens(sens))
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
    if (m_displayFlags & DisplayMotionSensitivity) 
    {
        QnMotionRegion& r = m_motionRegionList[channel];
        QRegion reg;
        for (int i = 0; i < 10; ++i) 
        {
            QColor clr = i > 0 ? QColor(100 +  i*3, 16*(10-i), 0, 96+i*2) : qnGlobals->motionMaskColor();
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

const QList<QnMotionRegion> &QnResourceWidget::motionRegionList()
{
    ensureMotionMask();
    return m_motionRegionList;
}

bool QnResourceWidget::isMotionSelectionEmpty() const { // TODO: create a separate class for a list of motion regions.
    foreach(const QRegion &region, motionSelection())
        if(!region.isEmpty())
            return false;
    return true;
}
