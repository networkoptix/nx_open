
#ifndef BASIC_AV_STREAM_READER_H
#define BASIC_AV_STREAM_READER_H

#ifdef ENABLE_ARECONT

#include <utils/common/util.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/random.h>

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

        this->m_streamParam.insert("image_left", 0);
        this->m_streamParam.insert("image_right", maxResolution.width());

        this->m_streamParam.insert("image_top", 0);
        this->m_streamParam.insert("image_bottom", maxResolution.height());

        this->m_streamParam.insert("streamID", nx::utils::random::number(1, 32000));

        this->m_streamParam.insert("resolution", lit("full"));
        this->m_streamParam.insert("Quality", lit("11"));

        QObject::connect(
            res.data(), &QnResource::initializedChanged,
            this, &QnBasicAvStreamReader<StreamProviderType>::at_resourceInitDone,
            Qt::DirectConnection);
    }

    QSize getMaxSensorSize() const
    {
        int val_w = this->getResource()->getProperty(lit("MaxSensorWidth")).toInt();
        int val_h = this->getResource()->getProperty(lit("MaxSensorHeight")).toInt();
        return (val_w && val_h) ? QSize(val_w, val_h) : QSize(0, 0);
    }

    virtual void pleaseReopenStream() override
    {
        QnMutexLocker mtx(&this->m_mutex);
        QnLiveStreamParams params = this->getLiveParams();
        QString resolution;
        if (this->getRole() == Qn::CR_LiveVideo)
            resolution = QLatin1String("full");
        else
            resolution = QLatin1String("half");

        QnResourcePtr res = this->getResource();
        QnPlAreconVisionResourcePtr avRes = res.staticCast<QnPlAreconVisionResource>();
        Qn::StreamQuality q = params.quality;
        switch (q)
        {
            case Qn::QualityHighest:
                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("resolution"), resolution);
                else
                    this->m_streamParam.insert("resolution", lit("full"));

                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("Quality"), QString::number(19)); // panoramic
                else
                    this->m_streamParam.insert("Quality", 19);
                break;

            case Qn::QualityHigh:
                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("resolution"), resolution);
                else
                    this->m_streamParam.insert("resolution", lit("full"));

                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("Quality"), QString::number(13)); // panoramic
                else
                    this->m_streamParam.insert("Quality", 13);
                break;

            case Qn::QualityNormal:
                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("resolution"), resolution);
                else
                    this->m_streamParam.insert("resolution", lit("full"));

                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("Quality"), QString::number(8)); // panoramic
                else
                    this->m_streamParam.insert("Quality", 8);
                break;

            case Qn::QualityLow:
                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("resolution"), resolution);
                else
                    this->m_streamParam.insert("resolution", lit("half"));

                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("Quality"), QString::number(4)); // panoramic
                else
                    this->m_streamParam.insert("Quality", 4);
                break;


            case Qn::QualityLowest:
                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("resolution"), resolution);
                else
                    this->m_streamParam.insert("resolution", QLatin1String("half"));

                if (avRes->isPanoramic())
                    avRes->setApiParameter(lit("Quality"), QString::number(1)); // panoramic
                else
                    this->m_streamParam.insert("Quality", 1);
                break;

            default:
                break;
        }
    }

    void at_resourceInitDone(const QnResourcePtr &resource)
    {
        if (resource->isInitialized())
        {
            QnMutexLocker lock(&this->m_needUpdateMtx);
            m_needUpdateParams = true;
        }
    }

    void updateCameraParams()
    {
        bool needUpdate = false;
        {
            QnMutexLocker lock(&m_needUpdateMtx);
            needUpdate = m_needUpdateParams;
            m_needUpdateParams = false;
        }

        if (needUpdate)
            pleaseReopenStream();
    }

private:
    bool m_needUpdateParams;
    mutable QnMutex m_needUpdateMtx;
};

#endif  //ENABLE_ARECONT

#endif  //BASIC_AV_STREAM_READER_H
