#include "live_stream_provider.h"
#include "media_streamdataprovider.h"
#include "cpull_media_stream_provider.h"
#include "core/resource/camera_resource.h"



QnLiveStreamProvider::QnLiveStreamProvider(QnResourcePtr res):
m_livemutex(QMutex::Recursive),
m_quality(QnQualityNormal),
m_fps(-1.0),
m_framesSinceLastMetaData(0),
m_role(QnResource::Role_LiveVideo),
m_softMotionLastChannel(0)
{
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
    
    {
        QMutexLocker mtx(&m_livemutex);
        m_role = role;
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
        if (m_quality == q)
            return; // same quality


        Q_ASSERT(m_role != QnResource::Role_SecondaryLiveVideo || q == QnQualityLowest); // trying to play with quality for second stream by yourself 

        m_quality = q;

    }

    updateStreamParamsBasedOnQuality();

}

QnStreamQuality QnLiveStreamProvider::getQuality() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_quality;
}

void QnLiveStreamProvider::updateSoftwareMotion()
{
    if (getRole() == QnResource::Role_SecondaryLiveVideo && m_cameraRes->getMotionType() == MT_SoftwareGrid)
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
        m_cameraRes->onPrimaryFpsUpdated(f);
    }



    if (QnClientPullMediaStreamProvider* cpdp = dynamic_cast<QnClientPullMediaStreamProvider*>(this))
    {
        // all client pull stream providers use the same mechanism 
        cpdp->setFps(f);
    }
    else
        updateStreamParamsBasedOnFps();
}

float QnLiveStreamProvider::getFps() const
{
    QMutexLocker mtx(&m_livemutex);

    if (m_fps < 0) // setfps never been called
        m_fps = m_cameraRes->getMaxFps() - 4; // 4 here is out of the blue; just somthing not maximum

    m_fps = qMin((int)m_fps, m_cameraRes->getMaxFps());
    m_fps = qMax((int)m_fps, 1);

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

    if (getRole() == QnResource::Role_SecondaryLiveVideo && m_cameraRes->getMotionType() == MT_SoftwareGrid)
    {
        for (int i = 0; i < m_layout->numberOfChannels(); ++i)
        {
            bool rez = m_motionEstimation[i].existsMetadata();
            if (rez) {
                m_softMotionLastChannel = i;
                return rez;
            }
        }
        return false;
    }
    else if (getRole() == QnResource::Role_LiveVideo && (m_cameraRes->getMotionType() == MT_HardwareGrid || m_cameraRes->getMotionType() == MT_MotionWindow))
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

    if (m_role == QnResource::Role_SecondaryLiveVideo && m_cameraRes->getMotionType() == MT_SoftwareGrid)
        m_motionEstimation[videoData->channelNumber].analizeFrame(videoData);
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

    StreamFpsSharingMethod sharingMethod = res->streamFpsSharingMethod();
    int newSecFps;

    if (sharingMethod == sharePixels)
        newSecFps = qMin(DESIRED_SECOND_STREAM_FPS, maxFps); //minimum between DESIRED_SECOND_STREAM_FPS and what is left;
    else if (sharingMethod == shareFps)
        newSecFps = qMin(DESIRED_SECOND_STREAM_FPS, maxFps - newFps); //ss; minimum between 5 and what is left;
    else// noSharing
        newSecFps = qMin(DESIRED_SECOND_STREAM_FPS, maxFps);




    //Q_ASSERT(newSecFps>=0); // default fps is 10. Some camers has lower fps and assert is appear

    setFps(qMax(1,newSecFps));
}

QnMetaDataV1Ptr QnLiveStreamProvider::getMetaData()
{
    if (m_cameraRes->getMotionType() == MT_SoftwareGrid)
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
