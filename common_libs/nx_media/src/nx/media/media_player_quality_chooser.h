#pragma once

#include <QtCore/QSize>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

#include <nx/streaming/media_data_packet.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

#include "media_player.h" //< For enum VideoQuality.

namespace nx {
namespace media {

/**
 * Mechanism which decides which video quality to apply to a stream reader based on user-preferred
 * quality and client/server trancoding/codec capabilities and available camera streams.
 */
namespace media_player_quality_chooser {

const QSize kQualityLow{-1, Player::LowVideoQuality}; //< Non-isValid() token-value.
const QSize kQualityHigh{-1, Player::HighVideoQuality}; //< Non-isValid() token-value.
const QSize kQualityLowIframesOnly{-1, Player::LowIframesOnlyVideoQuality}; //< Non-isValid() token-value.

/**
 * @param videoQuality Video quality desired by the user. Same as Player::videoQuality: either
 *     one of enum Player::VideoQuality values, or approximate vertical resolution.
 * @param liveMode Used to find a server to query transcoding capability.
 * @param positionMs Used when not liveMode, to find a server to query transcoding capability.
 * @return Either one of kQualityLow or kQialityHigh tokens, or a custom resolution which can
 *     have width set to <=0 to indicate "auto" width. ATTENTION: This method does not inspect
 *     camera aspect ratio, thus, the returned custom size width should be treated as specified
 *     in logical pixels.
 */
QSize chooseVideoQuality(
    AVCodecID transcodingCodec,
    int videoQuality,
    bool liveMode,
    qint64 positionMs,
    const QnVirtualCameraResourcePtr& camera);

} // namespace media_player_quality_chooser

} // namespace media
} // namespace nx
