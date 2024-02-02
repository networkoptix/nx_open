// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/codec/nal_units.h>
#include <nx/media/video_data_packet.h>

namespace nx::media::h264 {

NX_MEDIA_CORE_API std::vector<uint8_t> buildExtraDataAnnexB(const uint8_t* data, int32_t size);
NX_MEDIA_CORE_API std::vector<uint8_t> buildExtraDataMp4FromAnnexB(const uint8_t* data, int32_t size);
NX_MEDIA_CORE_API std::vector<uint8_t> buildExtraDataMp4(const std::vector<nal::NalUnitInfo>& nalUnits);

NX_MEDIA_CORE_API std::vector<nal::NalUnitInfo> decodeNalUnits(const QnCompressedVideoData* videoData);

NX_MEDIA_CORE_API bool extractSps(const QnCompressedVideoData* videoData, SPSUnit& sps);

} // namespace nx::media::h264
