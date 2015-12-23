#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

namespace nx
{
    namespace media
    {
        struct FrameMetadata
        {
            FrameMetadata();
            FrameMetadata(const QnConstCompressedVideoDataPtr& frame);
            void serialize(const QSharedPointer<QVideoFrame>& frame) const;
            static FrameMetadata deserialize(const QSharedPointer<const QVideoFrame>& frame);

            QnAbstractMediaData::MediaFlags flags;
        };
        Q_DECLARE_METATYPE(FrameMetadata);
    }
}
