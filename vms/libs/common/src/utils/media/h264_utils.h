#pragma once

#include <map>

#include <QtCore/QSize>
#include <QtCore/QString>

#include "nx/streaming/video_data_packet.h"
#include "utils/media/nalUnits.h"

namespace nx::media_utils {

void readNALUsFromAnnexBStream(
    const QnConstCompressedVideoDataPtr& data,
    std::vector<std::pair<const quint8*, size_t>>* const nalUnits);

void convertStartCodesToSizes(const uint8_t* data, int32_t size);

std::vector<uint8_t> buildExtraData(const uint8_t* data, int32_t size);

namespace h264 {

std::vector<std::pair<const quint8*, size_t>> decodeNalUnits(const QnConstCompressedVideoDataPtr& videoData);

bool extractSps(const QnConstCompressedVideoDataPtr& videoData, SPSUnit& sps);

} // namespace h264
} // namespace nx::media_utils

