#include "light_motion_archive_connection.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "utils/common/util.h"

QnLightMotionArchiveConnection::QnLightMotionArchiveConnection(const QnMetaDataLightVector& data, int channel):
    m_motionData(data),
    m_channel(channel)
{

}

QnMetaDataV1Ptr QnLightMotionArchiveConnection::getMotionData(qint64 timeUsec)
{
    if (m_motionData.empty() ||
            (m_lastResult && m_lastResult->containTime(timeUsec)))  //TODO: #rvasilenko Is this check sane? Why do we return empty result?
        return QnMetaDataV1Ptr();

    qint64 timeMs = timeUsec/1000;
    QnMetaDataLightVector::const_iterator itr = qUpperBound(m_motionData.begin(), m_motionData.end(), timeMs);
    if (itr != m_motionData.begin())
        --itr;
    //for (; itr != m_motionData.constEnd() && itr->channel != m_channel; ++itr);

    if (itr == m_motionData.end() || itr->endTimeMs() < timeMs)
    {
        if (itr != m_motionData.end())
            ++itr;
        qint64 lastEndTimeUsec = 0;
        if (m_lastResult)
            lastEndTimeUsec = m_lastResult->timestamp + m_lastResult->m_duration;
        m_lastResult = QnMetaDataV1Ptr(new QnMetaDataV1());
        m_lastResult->channelNumber = m_channel;
        m_lastResult->timestamp = lastEndTimeUsec;
        qint64 nextTimeUsec = itr != m_motionData.end() ? itr->startTimeMs*1000 : DATETIME_NOW;
        m_lastResult->m_duration = nextTimeUsec - m_lastResult->timestamp;
    }
    else {
        m_lastResult = QnMetaDataV1::fromLightData(*itr);
    }

    if (!m_lastResult->containTime(timeUsec))
        return QnMetaDataV1Ptr();

    return m_lastResult;
}

#endif // ENABLE_DATA_PROVIDERS
