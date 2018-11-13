#pragma once

#include <map>

#include <QtCore/QSize>
#include <QtCore/QString>

#include "nx/streaming/video_data_packet.h"
#include "utils/media/nalUnits.h"

namespace nx {
namespace media_utils {

void readNALUsFromAnnexBStream(
    const QnConstCompressedVideoDataPtr& data,
    std::vector<std::pair<const quint8*, size_t>>* const nalUnits);

namespace h264 {

std::vector<std::pair<const quint8*, size_t>> decodeNalUnits(const QnConstCompressedVideoDataPtr& videoData);

bool extractSps(const QnConstCompressedVideoDataPtr& videoData, SPSUnit& sps);

} // namespace h264
} // namespace media_utils
} // namespace nx
