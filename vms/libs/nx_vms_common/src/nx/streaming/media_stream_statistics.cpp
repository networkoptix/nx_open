// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_stream_statistics.h"

#include <nx/streaming/media_data_packet.h>

void QnMediaStreamStatistics::onData(const QnAbstractMediaDataPtr& media)
{
    const auto isKeyFrame = media->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey);
    const auto timestamp = std::chrono::microseconds(media->timestamp);
    return nx::sdk::MediaStreamStatistics::onData(timestamp, media->dataSize(), isKeyFrame);
}
