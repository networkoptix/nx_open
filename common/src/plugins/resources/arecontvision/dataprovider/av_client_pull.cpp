#include "av_client_pull.h"

#include "../resource/av_resource.h"
#include "utils/common/rand.h"

QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(QnResourcePtr res)
    : QnClientPullMediaStreamProvider(res),
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

    m_streamParam.insert("resolution", "full");
    m_streamParam.insert("Quality", "11");

    /**/

    setQuality(getQuality());  // to update stream params
}

QnPlAVClinetPullStreamReader::~QnPlAVClinetPullStreamReader()
{
    stop();
}

void QnPlAVClinetPullStreamReader::updateCameraMotion(const QnMotionRegion& region)
{
    static int sensToLevelThreshold[10] = 
    {
        31, // 0 - aka mask really filtered by media server always
        31, // 1
        25, // 2
        19, // 3
        14, // 4
        10, // 5
        7,  // 6
        5,  // 7
        3,  // 8
        2   // 9
    };

    for (int sens = QnMotionRegion::MIN_SENSITIVITY; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        if (sens != 0 && !region.getRegionBySens(sens).isEmpty())
        {
            QnPlAreconVisionResourcePtr avRes = getResource().dynamicCast<QnPlAreconVisionResource>();
            avRes->setParamAsync("mdlevelthreshold", sensToLevelThreshold[sens], QnDomainPhysical);
            break; // only 1 sensitivity for all frame is supported
        }
    }
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
            avRes->setParamAsync("resolution", "full", QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("full"));

        if (avRes->isPanoramic())
            avRes->setParamAsync("Quality", "19", QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 19);
        break;

    case QnQualityHigh:
        if (avRes->isPanoramic())
            avRes->setParamAsync("resolution", "full", QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("full"));

        if (avRes->isPanoramic())
            avRes->setParamAsync("Quality", "16", QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 16);
        break;

    case QnQualityNormal:
        if (avRes->isPanoramic())
            avRes->setParamAsync("resolution", "full", QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("full"));

        if (avRes->isPanoramic())
            avRes->setParamAsync("Quality", "13", QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 13);
        break;

    case QnQualityLow:
        if (avRes->isPanoramic())
            avRes->setParam("resolution", "half", QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("half"));

        if (avRes->isPanoramic())
            avRes->setParamAsync("Quality", "15", QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 15);
        break;


    case QnQualityLowest:
        if (avRes->isPanoramic())
            avRes->setParamAsync("resolution", "half", QnDomainPhysical);
        else
            m_streamParam.insert("resolution", QLatin1String("half"));

        if (avRes->isPanoramic())
            avRes->setParamAsync("Quality", "1", QnDomainPhysical); // panoramic
        else
            m_streamParam.insert("Quality", 1);
        break;

    default:
        break;
    }
}


int QnPlAVClinetPullStreamReader::getBitrate() const
{
    if (!getResource()->hasSuchParam("Bitrate"))
        return 0;

    QVariant val;
    getResource()->getParam("Bitrate", val, QnDomainMemory);
    return val.toInt();
}

bool QnPlAVClinetPullStreamReader::isH264() const
{
    if (!getResource()->hasSuchParam("Codec"))
        return false;

    QVariant val;
    getResource()->getParam("Codec", val, QnDomainMemory);
    return val==QLatin1String("H.264");
}
