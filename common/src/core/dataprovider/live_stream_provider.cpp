#include "live_stream_provider.h"
#include "media_streamdataprovider.h"
#include "cpull_media_stream_provider.h"
#include "core/resource/camera_resource.h"

int bestBitrateKbps(QnStreamQuality q, QSize resolution, int fps)
{
    // I assume for a good quality 30 fps for 640x480 we need 800kbps 

    int kbps = 400;

    switch (q)
    {
    case QnQualityHighest:
        kbps = 800;
    	break;

    case QnQualityHigh:
        kbps = 600;
        break;

    case QnQualityNormal:
        kbps = 450;
        break;

    case QnQualityLow:
        kbps = 300;
        break;

    case QnQualityLowest:
        kbps = 100;
        break;
    }

    float resolutionFactor = resolution.width()*resolution.height()/640.0/480.0;
    float frameRateFactor = fps/30.0;

    return kbps*resolutionFactor*frameRateFactor;
}


QnLiveStreamProvider::QnLiveStreamProvider():
m_quality(QnQualityNormal),
m_fps(-1.0),
m_framesSinceLastMetaData(0),
m_livemutex(QMutex::Recursive),
m_role(QnResource::Role_LiveVideo),
m_softwareMotion(false),
m_softMotionLastChannel(0)
{
    //m_softwareMotion = true;

    m_timeSinceLastMetaData.restart();

    char mask[MD_WIDTH * MD_HEIGHT];
    memset(mask, 10, sizeof(mask));
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionEstimation[i].setMotionMask(QByteArray(mask, sizeof(mask))); // default mask
}

QnLiveStreamProvider::~QnLiveStreamProvider()
{

}

void QnLiveStreamProvider::setRole(QnResource::ConnectionRole role)
{
    bool needUpdate = false;
    
    {
        QMutexLocker mtx(&m_livemutex);
        m_role = role;

        if (role == QnResource::Role_SecondaryLiveVideo)
        {
            if (m_quality != QnQualityLowest)
            {
                m_quality = QnQualityLowest;
                needUpdate = true;
            }
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

QByteArray QnLiveStreamProvider::createSoftwareMotionMask(const QnMotionRegion& region)
{
    static int sensitivityToMask[10] = 
    {
        255, //  0
        64,
        48,
        32,
        24,
        16,
        10,
        8,
        6,
        5, // 9
    };

    QByteArray rez;
    rez.resize(MD_WIDTH * MD_HEIGHT);
    memset(rez.data(), 255, MD_WIDTH * MD_HEIGHT);
    for (int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        foreach(const QRect& rect, region.getRegionBySens(sens).rects())
        {
            for (int y = rect.top(); y <= rect.bottom(); ++y)
            {
                for (int x = rect.left(); x <= rect.right(); ++x)
                {
                    rez.data()[x * MD_HEIGHT + y] = sensitivityToMask[sens];

                }

            }
        }
    }
    return rez;
}

void QnLiveStreamProvider::updateMotion()
{
    if (getRole() != QnResource::Role_LiveVideo)
        return;

    QnAbstractMediaStreamDataProvider* ap = dynamic_cast<QnAbstractMediaStreamDataProvider*>(this);
    Q_ASSERT(ap);

    QnPhysicalCameraResourcePtr res = ap->getResource().dynamicCast<QnPhysicalCameraResource>();
    Q_ASSERT(res);
    const QnVideoResourceLayout* layout = res->getVideoLayout();
    setUseSoftwareMotion(res->getMotionType() == MT_SoftwareGrid);
    if (res->getMotionType() == MT_SoftwareGrid)
    {
        for (int i = 0; i < layout->numberOfChannels(); ++i)
        {
            QnMotionRegion region = res->getMotionRegion(i);
            m_motionEstimation[i].setMotionMask(createSoftwareMotionMask(region));
        }
    }
    else {
        updateCameraMotion(res->getMotionRegion(0));
    }
}


// for live providers only
void QnLiveStreamProvider::setFps(float f)
{

    QnAbstractMediaStreamDataProvider* ap = dynamic_cast<QnAbstractMediaStreamDataProvider*>(this);
    Q_ASSERT(ap);

    QnPhysicalCameraResourcePtr res = ap->getResource().dynamicCast<QnPhysicalCameraResource>();
    Q_ASSERT(res);

    {
        QMutexLocker mtx(&m_livemutex);

        if (abs(m_fps - f) < 0.1)
            return; // same fps?


        m_fps = qMin((int)f, res->getMaxFps());
        f = m_fps;

    }
    
    if (getRole() != QnResource::Role_SecondaryLiveVideo)
    {
        // must be primary, so should inform secondary
        res->onPrimaryFpsUpdated(f);
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

    const QnAbstractMediaStreamDataProvider* ap = dynamic_cast<const QnAbstractMediaStreamDataProvider*>(this);
    Q_ASSERT(ap);

    QnPhysicalCameraResourcePtr res = ap->getResource().dynamicCast<QnPhysicalCameraResource>();
    Q_ASSERT(res);

    if (m_fps < 0) // setfps never been called
        m_fps = res->getMaxFps() - 4;

    m_fps = qMin((int)m_fps, res->getMaxFps());
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
    if (getRole() == QnResource::Role_SecondaryLiveVideo) 
    {
        if (m_softwareMotion) 
        {
            QnAbstractMediaStreamDataProvider* ap = dynamic_cast<QnAbstractMediaStreamDataProvider*>(this);
            QnPhysicalCameraResourcePtr res = ap->getResource().dynamicCast<QnPhysicalCameraResource>();
            const QnVideoResourceLayout* layout = res->getVideoLayout();

            for (int i = 0; i < layout->numberOfChannels(); ++i)
            {
                bool rez = m_motionEstimation[i].existsMetadata();
                if (rez) {
                    m_softMotionLastChannel = i;
                    return rez;
                }
            }
            return false;
        }
        else
            return false;
    }

    if (m_framesSinceLastMetaData == 0)
        return false;

    bool result = (m_framesSinceLastMetaData > 10 || m_timeSinceLastMetaData.elapsed() > META_DATA_DURATION_MS);

    if (result)
    {
        m_framesSinceLastMetaData = 0;
        m_timeSinceLastMetaData.restart();
    }
    return result;
}

void QnLiveStreamProvider::onGotVideoFrame(QnCompressedVideoDataPtr videoData)
{
    m_framesSinceLastMetaData++;

    if (m_role == QnResource::Role_SecondaryLiveVideo && m_softwareMotion)
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

    int newSecFps = qMin(5, maxFps - newFps);

    //Q_ASSERT(newSecFps>=0); // default fps is 10. Some camers has lower fps and assert is appear

    setFps(qMax(1,newSecFps));
}

QnMetaDataV1Ptr QnLiveStreamProvider::getMetaData()
{
    if (m_softwareMotion)
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

bool QnLiveStreamProvider::isSoftwareMotion() const
{
    return m_softwareMotion;
}

void QnLiveStreamProvider::setUseSoftwareMotion(bool value)
{
    m_softwareMotion = value;
}
