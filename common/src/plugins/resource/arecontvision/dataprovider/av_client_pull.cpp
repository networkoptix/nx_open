#ifdef ENABLE_ARECONT

#include "av_client_pull.h"

#include <utils/common/util.h>

#include "../resource/av_resource.h"

QSize QnPlAVClinetPullStreamReader::getMaxSensorSize(const QnResourcePtr& res) const 
{
    int val_w = res->getProperty(lit("MaxSensorWidth")).toInt();
    int val_h = res->getProperty(lit("MaxSensorHeight")).toInt();
    return (val_w && val_h) ? QSize(val_w, val_h) : QSize(0, 0);
}

QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(const QnResourcePtr& res):
    QnClientPullMediaStreamProvider(res),
    m_videoFrameBuff(CL_MEDIA_ALIGNMENT, 1024*1024),
    m_needUpdateParams(true)
{
    QSize maxResolution = getMaxSensorSize(res);

    //========this is due to bug in AV firmware;
    // you cannot set up maxSensorWidth with HTTP. ( you can do it in tftp if you are really want to ).
    // for now(13 December 2009) if we use HTTP => right should be divisible by 64; bottom - by 32
    // may be while you are looking at this comments bug already fixed.
    maxResolution.rwidth() = maxResolution.width()/64*64;
    maxResolution.rheight() = maxResolution.height()/32*32;

    m_streamParam.insert("image_left", 0);
    m_streamParam.insert("image_right", maxResolution.width());

    m_streamParam.insert("image_top", 0);
    m_streamParam.insert("image_bottom", maxResolution.height());

    m_streamParam.insert("streamID", random(1, 32000));

    m_streamParam.insert("resolution", QLatin1String("full"));
    m_streamParam.insert("Quality", QLatin1String("11"));

    /**/
    connect(res.data(), &QnResource::initializedChanged, this, &QnPlAVClinetPullStreamReader::at_resourceInitDone, Qt::DirectConnection);
    //setQuality(getQuality());  // to update stream params
}

void QnPlAVClinetPullStreamReader::at_resourceInitDone(const QnResourcePtr &resource)
{
    if (resource->isInitialized()) {
        SCOPED_MUTEX_LOCK( lock, &m_needUpdateMtx);
        m_needUpdateParams = true;
    }
}

void QnPlAVClinetPullStreamReader::updateCameraParams()
{
    bool needUpdate;
    {
        SCOPED_MUTEX_LOCK( lock, &m_needUpdateMtx);
        needUpdate = m_needUpdateParams;
        m_needUpdateParams = false;
    }

    if (needUpdate)
        updateStreamParamsBasedOnQuality();  // to update stream params
}

QnPlAVClinetPullStreamReader::~QnPlAVClinetPullStreamReader()
{
    stop();
}

void QnPlAVClinetPullStreamReader::updateStreamParamsBasedOnQuality()
{
    SCOPED_MUTEX_LOCK( mtx, &m_mutex);

    QString resolution;
    if (getRole() == Qn::CR_LiveVideo)
        resolution = QLatin1String("full");
    else
        resolution = QLatin1String("half");

    QnPlAreconVisionResourcePtr avRes = getResource().dynamicCast<QnPlAreconVisionResource>();
    Qn::StreamQuality q = getQuality();
    switch (q)
    {
    case Qn::QualityHighest:
        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("resolution"), resolution);
        else
            m_streamParam.insert("resolution", lit("full"));

        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("Quality"), QString::number(19)); // panoramic
        else
            m_streamParam.insert("Quality", 19);
        break;

    case Qn::QualityHigh:
        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("resolution"), resolution);
        else
            m_streamParam.insert("resolution", lit("full"));

        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("Quality"), QString::number(16)); // panoramic
        else
            m_streamParam.insert("Quality", 16);
        break;

    case Qn::QualityNormal:
        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("resolution"), resolution);
        else
            m_streamParam.insert("resolution", QLatin1String("full"));

        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("Quality"), QString::number(13)); // panoramic
        else
            m_streamParam.insert("Quality", 13);
        break;

    case Qn::QualityLow:
        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("resolution"), resolution);
        else
            m_streamParam.insert("resolution", QLatin1String("half"));

        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("Quality"), QString::number(15)); // panoramic
        else
            m_streamParam.insert("Quality", 10);
        break;


    case Qn::QualityLowest:
        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("resolution"), resolution);
        else
            m_streamParam.insert("resolution", lit("half"));

        if (avRes->isPanoramic())
            avRes->setParamPhysicalAsync(lit("Quality"), QString::number(1)); // panoramic
        else
            m_streamParam.insert("Quality", 1);
        break;

    default:
        break;
    }
}


int QnPlAVClinetPullStreamReader::getBitrate() const
{
    return getResource()->getProperty(lit("Bitrate")).toInt();
}

bool QnPlAVClinetPullStreamReader::isH264() const
{
    return getResource()->getProperty(lit("Codec")) == lit("H.264");
}

#endif
