#include "live_stream_provider.h"
#include "core/resource/camera_resource.h"


QnLiveStreamProvider::QnLiveStreamProvider(QnResourcePtr res):
    QnAbstractMediaStreamDataProvider(res),
    m_livemutex(QMutex::Recursive),
    m_quality(Qn::QualityNormal),
    m_qualityUpdatedAtLeastOnce(false),
    m_fps(-1.0),
    m_framesSinceLastMetaData(0),
    m_softMotionRole(QnResource::Role_Default),
    m_softMotionLastChannel(0),
    m_secondaryQuality(Qn::SSQualityNotDefined)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_motionMaskBinData[i] = (simd128i*) qMallocAligned(MD_WIDTH * MD_HEIGHT/8, 32);
        memset(m_motionMaskBinData[i], 0, MD_WIDTH * MD_HEIGHT/8);
    }

    m_role = QnResource::Role_LiveVideo;
    m_timeSinceLastMetaData.restart();
    m_layout = 0;
    m_cameraRes = res.dynamicCast<QnPhysicalCameraResource>();
    Q_ASSERT(m_cameraRes);
    m_prevCameraControlDisabled = m_cameraRes->isCameraControlDisabled();
    m_layout = m_cameraRes->getVideoLayout();
    m_isPhysicalResource = res.dynamicCast<QnPhysicalCameraResource>();
}

QnLiveStreamProvider::~QnLiveStreamProvider()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) 
        qFreeAligned(m_motionMaskBinData[i]);
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
            Qn::StreamQuality newQuality = m_cameraRes->getSecondaryStreamQuality();
            if (newQuality != m_quality)
            {
                m_quality = newQuality;
                needUpdate = true;
            }

            int oldFps = m_fps;
            int newFps = qMin(m_cameraRes->desiredSecondStreamFps(), m_cameraRes->getMaxFps()); //ss; target for secind stream fps is 5 
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

void QnLiveStreamProvider::setCameraControlDisabled(bool value)
{
    if (!value && m_prevCameraControlDisabled)
        updateStreamParamsBasedOnQuality();
    m_prevCameraControlDisabled = value;
}

void QnLiveStreamProvider::setSecondaryQuality(Qn::SecondStreamQuality  quality)
{
    {
        QMutexLocker mtx(&m_livemutex);
        if (m_secondaryQuality == quality)
            return; // same quality
        m_secondaryQuality = quality;
        if (m_secondaryQuality == Qn::SSQualityNotDefined)
            return;
    }

    if (getRole() != QnResource::Role_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary
        m_cameraRes->lockConsumers();
        foreach(QnResourceConsumer* consumer, m_cameraRes->getAllConsumers())
        {
            QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
            if (lp && lp->getRole() == QnResource::Role_SecondaryLiveVideo) {
                lp->setQuality(m_cameraRes->getSecondaryStreamQuality());
                lp->onPrimaryFpsUpdated(m_fps);
            }
        }
        m_cameraRes->unlockConsumers();

        updateStreamParamsBasedOnFps();
    }
}

void QnLiveStreamProvider::setQuality(Qn::StreamQuality q)
{
    {
        QMutexLocker mtx(&m_livemutex);
        if (m_quality == q && m_qualityUpdatedAtLeastOnce)
            return; // same quality

        m_quality = q;

        m_qualityUpdatedAtLeastOnce = true;

    }

    updateStreamParamsBasedOnQuality();
}

Qn::StreamQuality QnLiveStreamProvider::getQuality() const
{
    QMutexLocker mtx(&m_livemutex);
    return m_quality;
}

QnResource::ConnectionRole QnLiveStreamProvider::roleForMotionEstimation()
{
    if (m_softMotionRole == QnResource::Role_Default) {
        if (m_cameraRes && !m_cameraRes->hasDualStreaming2() && (m_cameraRes->getCameraCapabilities() & Qn::PrimaryStreamSoftMotionCapability))
            m_softMotionRole = QnResource::Role_LiveVideo;
        else
            m_softMotionRole = QnResource::Role_SecondaryLiveVideo;
    }
    return m_softMotionRole;
}

void QnLiveStreamProvider::onStreamResolutionChanged( int /*channelNumber*/, const QSize& /*picSize*/ )
{
}

void QnLiveStreamProvider::updateSoftwareMotion()
{
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid && getRole() == roleForMotionEstimation())
    {
        for (int i = 0; i < m_layout->channelCount(); ++i)
        {
            QnMotionRegion region = m_cameraRes->getMotionRegion(i);
            m_motionEstimation[i].setMotionMask(region);
        }
    }
#endif
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        QnMetaDataV1::createMask(m_cameraRes->getMotionMask(i), (char*)m_motionMaskBinData[i]);
}

bool QnLiveStreamProvider::canChangeStatus() const
{
    return m_role == QnResource::Role_LiveVideo && m_isPhysicalResource;
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
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
        if (getRole() == roleForMotionEstimation()) {
            for (int i = 0; i < m_layout->channelCount(); ++i)
            {
                bool rez = m_motionEstimation[i].existsMetadata();
                if (rez) {
                    m_softMotionLastChannel = i;
                    return rez;
                }
            }
        }
#endif
        return false;
    }
    else if (getRole() == QnResource::Role_LiveVideo && (m_cameraRes->getMotionType() == Qn::MT_HardwareGrid || m_cameraRes->getMotionType() == Qn::MT_MotionWindow))
    {
        bool result = (m_framesSinceLastMetaData > 10 ||
                       (m_framesSinceLastMetaData > 0 && m_timeSinceLastMetaData.elapsed() > META_DATA_DURATION_MS));
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

#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
    if (m_role == roleForMotionEstimation() && m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
        if( m_motionEstimation[videoData->channelNumber].analizeFrame(videoData) )
        {
            const QSize& newResolution = m_motionEstimation[videoData->channelNumber].videoResolution();
            if( newResolution != m_videoResolutionByChannelNumber[videoData->channelNumber] )
            {
                m_videoResolutionByChannelNumber[videoData->channelNumber] = newResolution;
                onStreamResolutionChanged( videoData->channelNumber, newResolution );
            }
        }
#endif
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
        newSecFps = qMin(m_cameraRes->desiredSecondStreamFps(), maxFps); //minimum between DESIRED_SECOND_STREAM_FPS and what is left;
        if (maxFps - newFps < 2 )
            newSecFps = qMin(m_cameraRes->desiredSecondStreamFps()/2,MIN_SECOND_STREAM_FPS);

    }
    else if (sharingMethod == Qn::shareFps)
        newSecFps = qMin(m_cameraRes->desiredSecondStreamFps(), maxFps - newFps); //ss; minimum between 5 and what is left;
    else// noSharing
        newSecFps = qMin(m_cameraRes->desiredSecondStreamFps(), maxFps);




    //Q_ASSERT(newSecFps>=0); // default fps is 10. Some camers has lower fps and assert is appear

    setFps(qMax(1,newSecFps));
}

QnMetaDataV1Ptr QnLiveStreamProvider::getMetaData()
{
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
    if (m_cameraRes->getMotionType() == Qn::MT_SoftwareGrid)
        return m_motionEstimation[m_softMotionLastChannel].getMotion();
    else
#endif
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

void QnLiveStreamProvider::startIfNotRunning()
{
    QMutexLocker mtx(&m_mutex);
    if (!isRunning())    
    {
        start();
    }
}


bool QnLiveStreamProvider::isCameraControlDisabled() const
{
    QnVirtualCameraResourcePtr camRes = m_resource.dynamicCast<QnVirtualCameraResource>();
    return camRes && camRes->isCameraControlDisabled();
}


void QnLiveStreamProvider::filterMotionByMask(QnMetaDataV1Ptr motion)
{
    motion->removeMotion(m_motionMaskBinData[motion->channelNumber]);
}
