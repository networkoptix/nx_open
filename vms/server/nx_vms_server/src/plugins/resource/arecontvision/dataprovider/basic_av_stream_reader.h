#pragma once

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
    QnBasicAvStreamReader(const QnPlAreconVisionResourcePtr& res)
    :
        StreamProviderType(res),
        m_camera(res)
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

        if (m_camera->isPanoramic())
            m_camera->setApiParameter("resolution", resolution);
        else
            this->m_streamParam.insert("resolution", resolution);

        Qn::StreamQuality q = params.quality;
        int avQuality = 0;
        switch (q)
        {
            case Qn::StreamQuality::highest:
                avQuality = 19;
                break;
            case Qn::StreamQuality::high:
                 avQuality = 13;
                break;
            case Qn::StreamQuality::normal:
                avQuality = 8;
                break;
            case Qn::StreamQuality::low:
                avQuality = 4;
                break;
            case Qn::StreamQuality::lowest:
                avQuality = 1;
                break;
            default:
                break;
        }
        if (avQuality > 0)
        {
            if (m_camera->isPanoramic())
                m_camera->setApiParameter("Quality", QString::number(avQuality));
            else
                this->m_streamParam.insert("Quality", avQuality);
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
protected:
    QnPlAreconVisionResourcePtr m_camera;
    QHash<QByteArray, QVariant> m_streamParam;

private:
    bool m_needUpdateParams = true;
    mutable QnMutex m_needUpdateMtx;
};

#endif  //ENABLE_ARECONT
