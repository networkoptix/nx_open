#include "media_resource_widget.h"

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


void QnMediaResourceWidget::updateChannelScreenSize(const QSize &channelScreenSize) {
    base_type::updateChannelScreenSize(channelScreenSize);

    m_renderer->setChannelScreenSize(m_channelScreenSize);
}