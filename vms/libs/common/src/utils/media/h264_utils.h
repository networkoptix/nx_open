#pragma once

#include <map>

#include <QtCore/QSize>
#include <QtCore/QString>

#include "nx/streaming/video_data_packet.h"
#include "utils/media/nalUnits.h"

namespace nx::media::h264 {

std::vector<uint8_t> buildExtraData(const uint8_t* data, int32_t size);

std::vector<nal::NalUnitInfo> decodeNalUnits(const QnConstCompressedVideoDataPtr& videoData);

bool extractSps(const QnConstCompressedVideoDataPtr& videoData, SPSUnit& sps);

} // namespace nx::media::h264

