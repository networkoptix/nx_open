#include "av_client_pull.h"

#include "../resource/av_resource.h"
#include "utils/common/rand.h"
#include "utils/common/base.h"

QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(QnResourcePtr res):
    QnClientPullMediaStreamProvider(res),
    QnLiveStreamProvider(res),
    m_videoFrameBuff(CL_MEDIA_ALIGNMENT, 1024*1024)
{
    QnSecurityCamResourcePtr ceqResource = getResource().dynamicCast<QnSecurityCamResource>();

    QSize maxResolution = ceqResource->getMaxSensorSize();

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

    m_streamParam.insert("streamID", (int)cl_get_random_val(1, 32000));

    m_streamParam.insert("resolution", QLatin1String("full"));
    m_streamParam.insert("Quality", QLatin1String("11"));

    /**/

    setQuality(getQuality());  // to update stream params
}

QnPlAVClinetPullStreamReader::~QnPlAVClinetPullStreamReader()
{
    stop();
}

void QnPlAVClinetPullStreamReader::updateStreamParamsBasedOnQuality()
{
    QMutexLocker mtx(&m_mutex);

    QnPlAreconVisionResourcePtr avRes = getResource().dynamicCast<QnPlAreconVisionResource>();
    QnStreamQuality q = getQuality();
    switch (q)
    {
    case QnQualityHighest:
        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("resolution"), QLatin1String("full"), QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("full"));

        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("Quality"), QLatin1String("19"), QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 19);
        break;

    case QnQualityHigh:
        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("resolution"), QLatin1String("full"), QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("full"));

        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("Quality"), QLatin1String("16"), QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 16);
        break;

    case QnQualityNormal:
        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("resolution"), QLatin1String("full"), QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("full"));

        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("Quality"), QLatin1String("13"), QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 13);
        break;

    case QnQualityLow:
        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("resolution"), QLatin1String("half"), QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("half"));

        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("Quality"), QLatin1String("15"), QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 15);
        break;


    case QnQualityLowest:
        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("resolution"), QLatin1String("half"), QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("half"));

        if (avRes->isPanoramic())
            avRes->setParamAsync(QLatin1String("Quality"), QLatin1String("1"), QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 1);
        break;

    default:
        break;
    }
}


int QnPlAVClinetPullStreamReader::getBitrate() const
{
    if (!getResource()->hasSuchParam(QLatin1String("Bitrate")))
        return 0;

    QVariant val;
    getResource()->getParam(QLatin1String("Bitrate"), val, QnDomainMemory);
    return val.toInt();
}

bool QnPlAVClinetPullStreamReader::isH264() const
{
    if (!getResource()->hasSuchParam(QLatin1String("Codec")))
        return false;

    QVariant val;
    getResource()->getParam(QLatin1String("Codec"), val, QnDomainMemory);
    return val==QLatin1String("H.264");
}
