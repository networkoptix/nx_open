#ifndef __GET_IMAGE_HELPER_H__
#define __GET_IMAGE_HELPER_H__

#include <QByteArray>

#include <api/helpers/thumbnail_request_data.h>

#include <core/resource/resource_fwd.h>
#include <camera/video_camera.h>

#include <transcoding/filters/abstract_image_filter.h>

class CLVideoDecoderOutput;
class QnServerArchiveDelegate;

class QnGetImageHelper
{
public:
    QnGetImageHelper() {}

    static QSharedPointer<CLVideoDecoderOutput> getImage(
          const QnSecurityCamResourcePtr &camera
        , qint64 timeUsec
        , const QSize& size
        , QnThumbnailRequestData::RoundMethod roundMethod = QnThumbnailRequestData::KeyFrameBeforeMethod
        , int rotation = QnThumbnailRequestData::kDefaultRotation);

    static QByteArray encodeImage(const QSharedPointer<CLVideoDecoderOutput>& outFrame, const QByteArray& format);

private:
    static QSharedPointer<CLVideoDecoderOutput> readFrame(
          qint64 timeUsec
        , bool useHQ
        , QnThumbnailRequestData::RoundMethod roundMethod
        , const QnSecurityCamResourcePtr &camera
        , QnServerArchiveDelegate &serverDelegate
        , int prefferedChannel);

    static QSize updateDstSize(const QSharedPointer<QnSecurityCamResource>& res, const QSize& dstSize, QSharedPointer<CLVideoDecoderOutput> outFrame);

    static QSharedPointer<CLVideoDecoderOutput> getImageWithCertainQuality(
          bool useHQ
        , const QnSecurityCamResourcePtr &camera
        , qint64 timeUsec
        , const QSize& size
        , QnThumbnailRequestData::RoundMethod roundMethod = QnThumbnailRequestData::KeyFrameBeforeMethod
        , int rotation = -1 );

    static QSharedPointer<CLVideoDecoderOutput> getMostPreciseImageFromLive(
        QnVideoCameraPtr& camera,
        bool useHq,
        quint64 time,
        int channel);

    static CLVideoDecoderOutputPtr decodeFrameSequence(
        std::unique_ptr<QnConstDataPacketQueue>& sequence,
        quint64 time);
};

#endif // __GET_IMAGE_HELPER_H__
