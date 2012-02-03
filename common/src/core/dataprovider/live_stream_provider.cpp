#include "live_stream_provider.h"
#include "media_streamdataprovider.h"
#include "cpull_media_stream_provider.h"
#include "core/resource/camera_resource.h"


QnLiveStreamProvider::QnLiveStreamProvider():
m_quality(QnQualityNormal),
m_fps(MAX_LIVE_FPS),
m_framesSinceLastMetaData(0),
m_livemutex(QMutex::Recursive),
m_role(QnResource::Role_LiveVideo)
{
    m_timeSinceLastMetaData.restart();
}

QnLiveStreamProvider::~QnLiveStreamProvider()
{

}

void QnLiveStreamProvider::setRole(QnResource::ConnectionRole role)
{
    QMutexLocker mtx(&m_livemutex);
    m_role = role;

    if (m_role == QnResource::Role_SecondaryLiveVideo)
        setQuality(QnQualityLowest);
}

QnResource::ConnectionRole QnLiveStreamProvider::getRole() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_role;
}


void QnLiveStreamProvider::setQuality(QnStreamQuality q)
{
    QMutexLocker mtx(&m_livemutex);
    if (m_quality == q)
        return; // same quality

    
    Q_ASSERT(m_role != QnResource::Role_SecondaryLiveVideo || q == QnQualityLowest); // trying to play with quality for second stream by yourself 

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

    if (abs(m_fps - f) < 0.1)
        return; // same fps?

    m_fps = f;
    
    if (getRole() != QnResource::Role_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary

        QnAbstractMediaStreamDataProvider* ap = dynamic_cast<QnAbstractMediaStreamDataProvider*>(this);
        Q_ASSERT(ap);

        QnPhysicalCameraResourcePtr res = ap->getResource().dynamicCast<QnPhysicalCameraResource>();
        Q_ASSERT(res);

        res->onPrimaryFpsUpdated(f);
    }



    if (QnClientPullMediaStreamProvider* cpdp = dynamic_cast<QnClientPullMediaStreamProvider*>(this))
    {
        // all client pull stream providers use the same mechanism 

        cpdp->setFps(f);
    }
    else
        updateStreamParamsBasedOnFps();

    emit fpsChanged();
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

bool QnLiveStreamProvider::needMetaData() 
{
    // I assume this function is called once per video frame 
    if (getRole() == QnResource::Role_SecondaryLiveVideo)
        return false;

    bool result = (m_framesSinceLastMetaData > 10 || m_timeSinceLastMetaData.elapsed() > META_DATA_DURATION_MS) &&
        m_framesSinceLastMetaData > 0; // got at least one frame

    if (result)
    {
        m_framesSinceLastMetaData = 0;
        m_timeSinceLastMetaData.restart();
    }
    return result;
}

void QnLiveStreamProvider::onGotFrame()
{
    m_framesSinceLastMetaData++;
}


void QnLiveStreamProvider::onPrimaryFpsUpdated(int newFps)
{
    Q_ASSERT(getRole() == QnResource::Role_SecondaryLiveVideo);
    // now primary has newFps
    // this is secondary stream
    // need to adjust fps 

    QnAbstractMediaStreamDataProvider* ap = dynamic_cast<QnAbstractMediaStreamDataProvider*>(this);
    Q_ASSERT(ap);

    QnPhysicalCameraResourcePtr res = ap->getResource().dynamicCast<QnPhysicalCameraResource>();
    Q_ASSERT(res);

    int maxFps = res->getMaxFps();

    int newSecFps = qMin(5, maxFps - newFps);

    Q_ASSERT(newSecFps>=1);

    setFps(newSecFps);
}
