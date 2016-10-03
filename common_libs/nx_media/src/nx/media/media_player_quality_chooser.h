#pragma once

#include <QtCore/QSize>

#include <nx/streaming/media_data_packet.h>
#include <core/resource/resource_fwd.h>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

namespace nx {
namespace media {

/**
 * Mechanism which decides which video quality to apply to a stream reader based on user-preferred
 * quality and client/server trancoding/codec capabilities and available camera streams.
 */
class media_player_quality_chooser
{
public:
    static const QSize kQualityLow; //< Token-value indicating low quality; isValid() is false.
    static const QSize kQualityHigh; //< Token-value indicating high quality; isValid() is false.

    /**
     * @param videoQuality Video quality desired by the user. Same as Player::videoQuality: either
     *     one of enum Player::VideoQuality values, or approximate vertical resolution.
     * @param liveMode Used to find a server to query transcoding capability.
     * @param positionMs Used when not liveMode, to find a server to query transcoding capability.
     * @return Either one of kQualityLow or kQialityHigh tokens, or a custom resolution which can
     *     have width set to <=0 to indicate "auto" width.
     */
    static QSize chooseVideoQuality(
        AVCodecID transcodingCodec,
        int videoQuality,
        bool liveMode,
        qint64 positionMs,
        const QnVirtualCameraResourcePtr& camera);

private:
    media_player_quality_chooser() = delete;

    /**
     * @return Transcoding resolution, or invalid (default) QSize if transcoding is not possible.
     */
    static QSize applyTranscodingIfPossible(
        AVCodecID transcodingCodec,
        bool liveMode, qint64 positionMs,
        const QnVirtualCameraResourcePtr& camera, const QSize& desiredResolution);
};

} // namespace media
} // namespace nx
