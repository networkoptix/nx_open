
#ifndef BASIC_AV_STREAM_READER_H
#define BASIC_AV_STREAM_READER_H

#include <utils/common/util.h>

#include "../resource/av_resource.h"


template<class StreamProviderType>
class QnBasicAvStreamReader
:
    public StreamProviderType
{
public:
    QnBasicAvStreamReader(const QnResourcePtr& res)
    :
        StreamProviderType(res),
        m_needUpdateParams(true)
    {
        QSize maxResolution = getMaxSensorSize();

        // ========this is due to bug in AV firmware;
        // you cannot set up maxSensorWidth with HTTP. ( you can do it in tftp if you are really want to ).
        // for now(13 December 2009) if we use HTTP => right should be divisible by 64; bottom - by 32
        // may be while you are looking at this comments bug already fixed.
        maxResolution.rwidth() = maxResolution.width() / 64 * 64;
        maxResolution.rheight() = maxResolution.height() / 32 * 32;

        m_streamParam.insert("image_left", 0);
        m_streamParam.insert("image_right", maxResolution.width());

        m_streamParam.insert("image_top", 0);
        m_streamParam.insert("image_bottom", maxResolution.height());

        m_streamParam.insert("streamID", random(1, 32000));

        m_streamParam.insert("resolution", QLatin1String("full"));
        m_streamParam.insert("Quality", QLatin1String("11"));

        QObject::connect(
            res.data(), &QnResource::initializedChanged,
            this, &QnBasicAvStreamReader<StreamProviderType>::at_resourceInitDone,
            Qt::DirectConnection);
    }

    QSize getMaxSensorSize() const
    {
        int val_w = getResource()->getProperty(lit("MaxSensorWidth")).toInt();
        int val_h = getResource()->getProperty(lit("MaxSensorHeight")).toInt();
        return (val_w && val_h) ? QSize(val_w, val_h) : QSize(0, 0);
    }

    virtual void pleaseReopenStream() override
    {
        QMutexLocker mtx(&m_mutex);
        QnLiveStreamParams params = getLiveParams();
        QString resolution;
        if (getRole() == Qn::CR_LiveVideo)
            resolution = QLatin1String("full");
        else
            resolution = QLatin1String("half");

        QnPlAreconVisionResourcePtr avRes = getResource().dynamicCast<QnPlAreconVisionResource>();
        Qn::StreamQuality q = params.quality;
        switch (q)
        {
            case Qn::QualityHighest:
                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("resolution"), resolution);
                else
                    m_streamParam.insert("resolution", QLatin1String("full"));

                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("Quality"), 19); // panoramic
                else
                    m_streamParam.insert("Quality", 19);
                break;

            case Qn::QualityHigh:
                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("resolution"), resolution);
                else
                    m_streamParam.insert("resolution", QLatin1String("full"));

                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("Quality"), 16); // panoramic
                else
                    m_streamParam.insert("Quality", 16);
                break;

            case Qn::QualityNormal:
                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("resolution"), resolution);
                else
                    m_streamParam.insert("resolution", QLatin1String("full"));

                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("Quality"), 13); // panoramic
                else
                    m_streamParam.insert("Quality", 13);
                break;

            case Qn::QualityLow:
                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("resolution"), resolution);
                else
                    m_streamParam.insert("resolution", QLatin1String("half"));

                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("Quality"), 15); // panoramic
                else
                    m_streamParam.insert("Quality", 10);
                break;


            case Qn::QualityLowest:
                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("resolution"), resolution);
                else
                    m_streamParam.insert("resolution", QLatin1String("half"));

                if (avRes->isPanoramic())
                    avRes->setParamPhysicalAsync(lit("Quality"), 1); // panoramic
                else
                    m_streamParam.insert("Quality", 1);
                break;

            default:
                break;
        }
    }

    void at_resourceInitDone(const QnResourcePtr &resource)
    {
        if (resource->isInitialized())
        {
            QMutexLocker lock(&m_needUpdateMtx);
            m_needUpdateParams = true;
        }
    }

    void updateCameraParams()
    {
        bool needUpdate = false;
        {
            QMutexLocker lock(&m_needUpdateMtx);
            needUpdate = m_needUpdateParams;
            m_needUpdateParams = false;
        }

        if (needUpdate)
            pleaseReopenStream();
    }

private:
    bool m_needUpdateParams;
    mutable QMutex m_needUpdateMtx;
};

#endif  //BASIC_AV_STREAM_READER_H
