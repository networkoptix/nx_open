#include "cpull_media_stream_provider.h"
#include <media_server/media_server_module.h>
#include <core/resource/resource_command_processor.h>

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/common/sleep.h>
#include <nx/utils/log/log.h>

#include <nx/streaming/video_data_packet.h>
#include <core/resource/camera_resource.h>

namespace {

    static const int kErrorDelayTimeoutMs = 100;

} // namespace

QnClientPullMediaStreamProvider::QnClientPullMediaStreamProvider(const nx::vms::server::resource::CameraPtr& dev)
    :
    QnLiveStreamProvider(dev),
    m_fpsSleep(100*1000)
{
}

void QnClientPullMediaStreamProvider::run()
{
    initSystemThreadId();
    setPriority(QThread::HighPriority);
    NX_VERBOSE(this, "stream reader started");

    int numberOfChnnels = 1;

    if (QnMediaResource* mr = dynamic_cast<QnMediaResource*>(m_resource.data()))
    {
        numberOfChnnels = mr->getVideoLayout()->channelCount();
    }

    beforeRun();

    while(!needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        // check queue sizes

        if (!dataCanBeAccepted())
        {
            QnSleep::msleep(5);
            continue;
        }

        // If command processor has something in the queue for this resource let it go first
        if (!m_resource->isInitialized())
        {
            QnSleep::msleep(5);
            continue;
        }

        QnAbstractMediaDataPtr data = getNextData();

        if (data == nullptr)
        {
            setNeedKeyData();
            m_stat[0].onEvent(CameraDiagnostics::BadMediaStreamResult());
            QnSleep::msleep(kErrorDelayTimeoutMs);
            continue;
        }

        if (getResource()->hasFlags(Qn::local_live_cam)) // for all local live cam add MediaFlags_LIVE flag;
        {
            data->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        }
        checkTime(data);

        QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(data);

        if (videoData && needKeyData())
        {
            // I do not like; need to do smth with it
            if (videoData->flags & AV_PKT_FLAG_KEY)
            {
                if (videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
                {
                    NX_ASSERT(false);
                    continue;
                }

                m_gotKeyFrame[videoData->channelNumber]++;
            }
            else
            {
                // need key data but got not key data
                continue;
            }
        }

        if(data)
            data->dataProvider = this;

        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(this);
        if (videoData)
        {
            m_stat[videoData->channelNumber].onData(
                static_cast<unsigned int>(videoData->dataSize()),
                videoData->flags & AV_PKT_FLAG_KEY);

            if (lp)
                lp->onGotVideoFrame(videoData, getLiveParams(), isCameraControlRequired());

        }

        if (data && lp && lp->getRole() == Qn::CR_SecondaryLiveVideo)
            data->flags |= QnAbstractMediaData::MediaFlags_LowQuality;


        putData(std::move(data));

        if (videoData && !isMaxFps()) {
            qint64 sleepTimeUsec = 1000*1000ll / getLiveParams().fps;
            m_fpsSleep.sleep(sleepTimeUsec / numberOfChnnels); // reduce sleep time if camera has several channels to archive requested fps per channel
        }

    }

    afterRun();

    NX_VERBOSE(this, "stream reader stopped");
}

void QnClientPullMediaStreamProvider::beforeRun()
{
    QnAbstractMediaStreamDataProvider::beforeRun();
    getResource()->init();
}

#endif // ENABLE_DATA_PROVIDERS
