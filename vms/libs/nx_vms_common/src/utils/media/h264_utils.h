// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/streaming/video_data_packet.h>
#include <utils/media/nalUnits.h>

namespace nx::media::h264 {

NX_VMS_COMMON_API std::vector<uint8_t> buildExtraDataAnnexB(const uint8_t* data, int32_t size);
NX_VMS_COMMON_API std::vector<uint8_t> buildExtraDataMp4(const uint8_t* data, int32_t size);

NX_VMS_COMMON_API std::vector<nal::NalUnitInfo> decodeNalUnits(const QnCompressedVideoData* videoData);

NX_VMS_COMMON_API bool extractSps(const QnCompressedVideoData* videoData, SPSUnit& sps);

} // namespace nx::media::h264

