#include "live_stream_provider.h"
#include "core/resource/camera_resource.h"


QnLiveStreamProvider::QnLiveStreamProvider(QnResourcePtr res):
QnAbstractMediaStreamDataProvider(res),
m_livemutex(QMutex::Recursive),
m_quality(QnQualityNormal),
m_qualityUpdatedAtLeastOnce(false),
m_fps(-1.0),
m_framesSinceLastMetaData(0),
m_softMotionRole(QnResource::Role_Default),
m_softMotionLastChannel(0)
{
    m_role = QnResource::Role_LiveVideo;
    m_timeSinceLastMetaData.restart();
    m_layout = 0;
    m_cameraRes = res.dynamicCast<QnPhysicalCameraResource>();
    Q_ASSERT(m_cameraRes);
    m_layout = m_cameraRes->getVideoLayout();
    m_isPhysicalResource = res.dynamicCast<QnPhysicalCameraResource>();
}

QnLiveStreamProvider::~QnLiveStreamProvider()
{
    //if (cameraRes->getMaxFps() - currentFps >= MIN_SECONDARY_FPS)
}

void QnLiveStreamProvider::setRole(QnResource::ConnectionRole role)
{
    bool needUpdate = false;
    QnAbstractMediaStreamDataProvider::setRole(role);
    {
        QMutexLocker mtx(&m_livemutex);
        updateSoftwareMotion();

        if (role == QnResource::Role_SecondaryLiveVideo)
        {
            if (m_quality != QnQualityLowest)
            {
                m_quality = QnQualityLowest;
                needUpdate = true;
            }

            int oldFps = m_fps;
            int newFps = qMin(DESIRED_SECOND_STREAM_FPS, m_cameraRes->getMaxFps()); //ss; target for secind stream fps is 5 
            newFps = qMax(1, newFps);

            if (newFps != oldFps)
            {
                needUpdate = true;
            }

            m_fps = newFps;
        }

    }

    if (needUpdate)
        updateStreamParamsBasedOnQuality();
}

QnResource::ConnectionRole QnLiveStreamProvider::getRole() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_role;
}


void QnLiveStreamProvider::setQuality(QnStreamQuality q)
{
    {
        QMutexLocker mtx(&m_livemutex);
        if (m_quality == q && m_qualityUpdatedAtLeastOnce)
            return; // same quality


        Q_ASSERT(m_role != QnResource::Role_SecondaryLiveVideo || q == QnQualityLowest); // trying to play with quality for second stream by yourself 

        m_quality = q;

        m_qualityUpdatedAtLeastOnce = true;

    }

    updateStreamParamsBasedOnQuality();

}

QnStreamQuality QnLiveStreamProvider::getQuality() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_quality;
}

QnResource::ConnectionRole QnLiveStreamProvider::roleForMotionEstimation()
{
    if (m_softMotionRole == QnResource::Role_Default) {
        if (m_cameraRes && !m_cameraRes->hasDualStreaming() && (m_cameraRes->getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability))
            m_softMotionRole = QnResource::Role_LiveVideo;
        else
            m_softMotionRole = QnResource::Role_SecondaryLiveVideo;
    }
    return m_softMotionRole;
}

void QnLiveStreamProvider::updateSoftwareMotion()
{
    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid && getRole() == roleForMotionEstimation())
    {
        for (int i = 0; i < m_layout->numberOfChannels(); ++i)
        {
            QnMotionRegion region = m_cameraRes->getMotionRegion(i);
            m_motionEstimation[i].setMotionMask(region);
        }
    }
}

// for live providers only
void QnLiveStreamProvider::setFps(float f)
{
    {
        QMutexLocker mtx(&m_livemutex);

        if (abs(m_fps - f) < 0.1)
            return; // same fps?


        m_fps = qMin((int)f, m_cameraRes->getMaxFps());
        f = m_fps;

    }
    
    if (getRole() != QnResource::Role_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary
        m_cameraRes->lockConsumers();
        foreach(QnResourceConsumer* consumer, m_cameraRes->getAllConsumers())
        {
            QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
            if (lp && lp->getRole() == QnResource::Role_SecondaryLiveVideo)
                lp->onPrimaryFpsUpdated(f);
        }
        m_cameraRes->unlockConsumers();
    }


    updateStreamParamsBasedOnFps();
}

float QnLiveStreamProvider::getFps() const
{
    QMutexLocker mtx(&m_livemutex);

    if (m_fps < 0) // setfps never been called
        m_fps = m_cameraRes->getMaxFps() - 2; // 2 here is out of the blue; just somthing not maximum

    m_fps = qMin((int)m_fps, m_cameraRes->getMaxFps());
    m_fps = qMax((int)m_fps, 1);

    return m_fps;
}

bool QnLiveStreamProvider::isMaxFps() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_fps >= m_cameraRes->getMaxFps()-0.1;
}

bool QnLiveStreamProvider::needMetaData() 
{
    // I assume this function is called once per video frame 

    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
    {
        if (getRole() == roleForMotionEstimation()) {
            for (int i = 0; i < m_layout->numberOfChannels(); ++i)
            {
                bool rez = m_motionEstimation[i].existsMetadata();
                if (rez) {
                    m_softMotionLastChannel = i;
                    return rez;
                }
            }
        }
        return false;
    }
    else if (getRole() == QnResource::Role_LiveVideo && (m_cameraRes->getMotionType() == Qn::MT_HardwareGrid || m_cameraRes->getMotionType() == Qn::MT_MotionWindow))
    {
        bool result = (m_framesSinceLastMetaData > 10 || m_timeSinceLastMetaData.elapsed() > META_DATA_DURATION_MS);
        if (result)
        {
            m_framesSinceLastMetaData = 0;
            m_timeSinceLastMetaData.restart();
        }
        return result;
    }
    
    return false; // not motion configured
}

void QnLiveStreamProvider::onGotVideoFrame(QnCompressedVideoDataPtr videoData)
{
    m_framesSinceLastMetaData++;

    if (m_role == roleForMotionEstimation() && m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
        m_motionEstimation[videoData->channelNumber].analizeFrame(videoData);
}

void QnLiveStreamProvider::onPrimaryFpsUpdated(int newFps)
{
    Q_ASSERT(getRole() == QnResource::Role_SecondaryLiveVideo);
    // now primary has newFps
    // this is secondary stream
    // need to adjust fps 

    int maxFps = m_cameraRes->getMaxFps();

    Qn::StreamFpsSharingMethod sharingMethod = m_cameraRes->streamFpsSharingMethod();
    int newSecFps;

    if (secondaryResolutionIsLarge())
        newSecFps = MIN_SECOND_STREAM_FPS;
    else if (sharingMethod == Qn::sharePixels)
    {
        newSecFps = qMin(DESIRED_SECOND_STREAM_FPS, maxFps); //minimum between DESIRED_SECOND_STREAM_FPS and what is left;
        if (maxFps - newFps < 2 )
            newSecFps = qMin(DESIRED_SECOND_STREAM_FPS/2,MIN_SECOND_STREAM_FPS);

    }
    else if (sharingMethod == Qn::shareFps)
        newSecFps = qMin(DESIRED_SECOND_STREAM_FPS, maxFps - newFps); //ss; minimum between 5 and what is left;
    else// noSharing
        newSecFps = qMin(DESIRED_SECOND_STREAM_FPS, maxFps);




    //Q_ASSERT(newSecFps>=0); // default fps is 10. Some camers has lower fps and assert is appear

    setFps(qMax(1,newSecFps));
}

QnMetaDataV1Ptr QnLiveStreamProvider::getMetaData()
{
    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
        return m_motionEstimation[m_softMotionLastChannel].getMotion();
    else
        return getCameraMetadata();
}

QnMetaDataV1Ptr QnLiveStreamProvider::getCameraMetadata()
{
    QnMetaDataV1Ptr result(new QnMetaDataV1(1));
    result->m_duration = 1000*1000*1000; // 1000 sec 
    return result;
}

bool QnLiveStreamProvider::hasRunningLiveProvider(QnNetworkResourcePtr netRes)
{
    bool rez = false;
    netRes->lockConsumers();
    foreach(QnResourceConsumer* consumer, netRes->getAllConsumers())
    {
        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
        if (lp)
        {
            QnLongRunnable* lr = dynamic_cast<QnLongRunnable*>(lp);
            if (lr && lr->isRunning()) {
                rez = true;
                break;
            }
        }
    }

    netRes->unlockConsumers();
    return rez;
}
