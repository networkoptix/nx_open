#include "get_image_helper.h"
#include "utils/media/frame_info.h"
#include "transcoding/transcoder.h"
#include "transcoding/filters/tiled_image_filter.h"
#include "transcoding/filters/scale_image_filter.h"
#include "transcoding/filters/rotate_image_filter.h"
#include "core/resource/camera_resource.h"
#include "camera/camera_pool.h"
#include "plugins/resource/server_archive/server_archive_delegate.h"
#include "decoders/video/ffmpeg.h"

static const int MAX_GOP_LEN = 100;

QnCompressedVideoDataPtr getNextArchiveVideoPacket(QnServerArchiveDelegate& serverDelegate, qint64 ceilTime)
{
    QnCompressedVideoDataPtr video;
    for (int i = 0; i < 20 && !video; ++i) 
    {
        QnAbstractMediaDataPtr media = serverDelegate.getNextData();
        if (!media || media->timestamp == DATETIME_NOW)
            break;
        video = media.dynamicCast<QnCompressedVideoData>();
    }

    // if ceilTime specified try frame with time > requested time (round time to ceil)
    if (ceilTime != (qint64)AV_NOPTS_VALUE && video && video->timestamp < ceilTime - 1000ll)
    {
        for (int i = 0; i < MAX_GOP_LEN; ++i) 
        {
            QnAbstractMediaDataPtr media2 = serverDelegate.getNextData();
            if (!media2 || media2->timestamp == DATETIME_NOW)
                break;
            QnCompressedVideoDataPtr video2 = media2.dynamicCast<QnCompressedVideoData>();
            if (video2 && (video2->flags & AV_PKT_FLAG_KEY))
                return video2;
        }
    }

    return video;
}

QSize QnGetImageHelper::updateDstSize(const QSharedPointer<QnVirtualCameraResource>& res, const QSize& srcSize, QSharedPointer<CLVideoDecoderOutput> outFrame)
{
    QSize dstSize(srcSize);
    double sar = outFrame->sample_aspect_ratio;
    double ar = sar * outFrame->width / outFrame->height;
    if (!dstSize.isEmpty()) {
        dstSize.setHeight(qPower2Ceil((unsigned) dstSize.height(), 4));
        dstSize.setWidth(qPower2Ceil((unsigned) dstSize.width(), 4));
    }
    else if (dstSize.height() > 0) {
        dstSize.setHeight(qPower2Ceil((unsigned) dstSize.height(), 4));
        dstSize.setWidth(qPower2Ceil((unsigned) (dstSize.height()*ar), 4));
    }
    else if (dstSize.width() > 0) {
        dstSize.setWidth(qPower2Ceil((unsigned) dstSize.width(), 4));
        dstSize.setHeight(qPower2Ceil((unsigned) (dstSize.width()/ar), 4));
    }
    else {
        dstSize = QSize(outFrame->width * sar, outFrame->height);
    }
    dstSize.setWidth(qMin(dstSize.width(), outFrame->width * sar));
    dstSize.setHeight(qMin(dstSize.height(), outFrame->height));

    qreal customAR = res->getProperty(QnMediaResource::customAspectRatioKey()).toDouble();
    if (customAR != 0.0)
        dstSize.setWidth(dstSize.height() * customAR);

    return QnCodecTranscoder::roundSize(dstSize);
}


QSharedPointer<CLVideoDecoderOutput> QnGetImageHelper::readFrame(qint64 time, 
                                                                   bool useHQ, 
                                                                   RoundMethod roundMethod, 
                                                                   const QSharedPointer<QnVirtualCameraResource>& res, 
                                                                   QnServerArchiveDelegate& serverDelegate, 
                                                                   int prefferedChannel)
{
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);

    QSharedPointer<CLVideoDecoderOutput> outFrame( new CLVideoDecoderOutput() );

    QnConstCompressedVideoDataPtr video;
    if (time == DATETIME_NOW) 
    {
        // get live data
        if (camera)
            video = camera->getLastVideoFrame(useHQ, prefferedChannel);
    }
    else if (time == LATEST_IMAGE)
    {
        // get latest data
        if (camera) {
            video = camera->getLastVideoFrame(useHQ, prefferedChannel);
            if (!video)
                video = camera->getLastVideoFrame(!useHQ, prefferedChannel);
        }
        if (!video) {
            video = camera->getLastVideoFrame(!useHQ, prefferedChannel);
            if (!serverDelegate.isOpened()) {
                serverDelegate.open(res);
                serverDelegate.seek(serverDelegate.endTime()-1000*100, true);
            }
            video = getNextArchiveVideoPacket(serverDelegate, AV_NOPTS_VALUE);
        }
        else {
            time = DATETIME_NOW;
        }
    }
    else {
        // get archive data
        if (!serverDelegate.isOpened()) {
            serverDelegate.open(res);
            serverDelegate.seek(time, true);
        }
        video = getNextArchiveVideoPacket(serverDelegate, roundMethod == IFrameAfterTime ? time : AV_NOPTS_VALUE);

        if (!video) {
            video = camera->getFrameByTime(useHQ, time, roundMethod == IFrameAfterTime, prefferedChannel); // try approx frame from GOP keeper
            time = DATETIME_NOW;
        }
        if (!video)
            video = camera->getFrameByTime(!useHQ, time, roundMethod == IFrameAfterTime, prefferedChannel); // try approx frame from GOP keeper
    }
    if (!video) 
        return QSharedPointer<CLVideoDecoderOutput>();

    CLFFmpegVideoDecoder decoder(video->compressionType, video, false);
    bool gotFrame = false;

    if (time == DATETIME_NOW) {
        if (res->getStatus() == Qn::Online || res->getStatus() == Qn::Recording)
        {
            gotFrame = decoder.decode(video, &outFrame);
            if (!gotFrame)
                gotFrame = decoder.decode(video, &outFrame); // decode twice
        }
    }
    else {
        bool precise = roundMethod == Precise;
        for (int i = 0; i < MAX_GOP_LEN && !gotFrame && video; ++i)
        {
            gotFrame = decoder.decode(video, &outFrame) && (!precise || video->timestamp >= time);
            if (gotFrame)
                break;
            video = getNextArchiveVideoPacket(serverDelegate, AV_NOPTS_VALUE);
        }
    }
    if (video)
        outFrame->channel = video->channelNumber;
    return outFrame;
}

QSharedPointer<CLVideoDecoderOutput> QnGetImageHelper::getImage(const QnVirtualCameraResourcePtr& res, qint64 time, const QSize& size, RoundMethod roundMethod, int rotation)
{
    if (!res)
        return QSharedPointer<CLVideoDecoderOutput>();

    QSize dstSize(size);
#ifdef EDGE_SERVER
    if (dstSize.height() < 1)
        dstSize.setHeight(360); //on edge instead of full-size image we return 360p
#endif

    bool useHQ = true;
    if ((dstSize.width() > 0 && dstSize.width() <= 480) || (dstSize.height() > 0 && dstSize.height() <= 316))
        useHQ = false;

    if (rotation == -1)
        rotation = res->getProperty(QnMediaResource::rotationKey()).toInt();

    QnServerArchiveDelegate serverDelegate;
    if (!useHQ)
        serverDelegate.setQuality(MEDIA_Quality_Low, true);

    QnConstResourceVideoLayoutPtr layout = res->getVideoLayout();
    QList<QnAbstractImageFilterPtr> filterChain;

    CLVideoDecoderOutputPtr outFrame;
    int channelMask = (1 << layout->channelCount()) -1;
    bool gotNullFrame = false;
    for (int i = 0; i < layout->channelCount(); ++i) 
    {
        CLVideoDecoderOutputPtr frame = readFrame(time, useHQ, roundMethod, res, serverDelegate, i);
        if (!frame) {
            gotNullFrame = true;
            if (i == 0)
                return QSharedPointer<CLVideoDecoderOutput>();
            else
                continue;
        }
        channelMask &= ~(1 << frame->channel);
        if (i == 0) {
            dstSize = updateDstSize(res, dstSize, frame);
            if (dstSize.width() <= 16 || dstSize.height() <= 8)
                return CLVideoDecoderOutputPtr();
            filterChain << QnAbstractImageFilterPtr(new QnScaleImageFilter(dstSize));
            filterChain << QnAbstractImageFilterPtr(new QnTiledImageFilter(layout));
            filterChain << QnAbstractImageFilterPtr(new QnRotateImageFilter(rotation));
        }
        for(auto filter: filterChain)
            frame = filter->updateImage(frame);
        if (frame)
            outFrame = frame;
    }
    // read more archive frames to get all channels for pano cameras
    if (channelMask && !gotNullFrame)
    {
        for (int i = 0; i < 10; ++i) {
            CLVideoDecoderOutputPtr frame = readFrame(time, useHQ, roundMethod, res, serverDelegate, 0);
            if (frame) {
                channelMask &= ~(1 << frame->channel);
                for(auto filter: filterChain)
                    frame = filter->updateImage(frame);
                outFrame = frame;
            }
        }
    }
    return outFrame;
}
