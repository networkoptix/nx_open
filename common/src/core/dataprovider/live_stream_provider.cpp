#include "live_stream_provider.h"


QnLiveStreamProvider::QnLiveStreamProvider():
m_quality(QnQualityNormal),
m_fps(MAX_LIVE_FPS),
m_framesSinceLastMetaData(0)

{
    m_timeSinceLastMetaData.restart();
}

void QnLiveStreamProvider::setQuality(QnStreamQuality q)
{
    QMutexLocker mtx(&m_livemutex);
    m_quality = q;
    updateStreamParamsBasedOnQuality();
    //setNeedKeyData();
}

QnStreamQuality QnLiveStreamProvider::getQuality() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_quality;
}

// for live providers only
void QnLiveStreamProvider::setFps(float f)
{
    QMutexLocker mtx(&m_livemutex);
    m_fps = f;
    updateStreamParamsBasedOnFps();
}

float QnLiveStreamProvider::getFps() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_fps;
}

bool QnLiveStreamProvider::isMaxFps() const
{
    QMutexLocker mtx(&m_livemutex);
    return abs( m_fps - MAX_LIVE_FPS)< .1;
}

bool QnLiveStreamProvider::needMetaData() const
{
    return (m_framesSinceLastMetaData > 10 || m_timeSinceLastMetaData.elapsed() > META_DATA_DURATION_MS) &&
        m_framesSinceLastMetaData > 0; // got at least one frame
}
