// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <boost/operators.hpp>

#include <QtCore/QSize>

#include <nx/streaming/media_data_packet.h>
#include <nx/media/media_player.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

namespace nx {
namespace media {

class AbstractVideoDecoder;

/**
 * Mechanism which decides which video quality to apply to a stream reader based on user-preferred
 * quality and client/server trancoding/codec capabilities and available camera streams.
 */
namespace media_player_quality_chooser {

enum class TranscodingRequestType
{
    detailed, //< Return status explaining why transcoding is not supported.
    simple //< Return only TranscodingSupported or TranscodingNotSupported or TranscodingDisabled.
};

Player::TranscodingSupportStatus transcodingSupportStatus(
    const QnVirtualCameraResourcePtr& camera,
    qint64 positionMs,
    bool liveMode,
    TranscodingRequestType requestType = TranscodingRequestType::detailed);

struct Params
{
    AVCodecID transcodingCodec = AV_CODEC_ID_NONE;

    /** Used to find a server to query transcoding capability. */
    bool liveMode = false;

    /** Used when not liveMode, to find a Server to query transcoding capability. */
    qint64 positionMs = -1;

    QnVirtualCameraResourcePtr camera;

    bool allowOverlay = false;

    bool allowHardwareAcceleration = false;

    /** List of decoders currently used by the player. */
    const std::vector<AbstractVideoDecoder*>* currentDecoders = nullptr;
};

struct Result: public boost::equality_comparable1<Result>
{
    Player::VideoQuality quality = Player::UnknownVideoQuality;
    QSize frameSize;

    Result(
        Player::VideoQuality quality = Player::UnknownVideoQuality,
        const QSize& frameSize = QSize());

    bool isValid() const;

    bool operator==(const Result& other) const;

    QString toString() const;
};

/**
 * @param videoQuality Video quality desired by the user. Same as Player::videoQuality: either
 *     one of enum Player::VideoQuality values, or approximate vertical resolution.
 * @return Either one of kQualityLow or kQualityHigh tokens, or a custom resolution which can
 *     have width set to <=0 to indicate "auto" width. ATTENTION: This method does not inspect
 *     camera aspect ratio, thus, the returned custom size width should be treated as specified
 *     in logical pixels.
 */
Result chooseVideoQuality(int videoQuality, const Params& input);

} // namespace media_player_quality_chooser

} // namespace media
} // namespace nx
