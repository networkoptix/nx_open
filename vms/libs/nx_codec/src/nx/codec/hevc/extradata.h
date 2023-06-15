// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <vector>

#include <nx/codec/nal_units.h>

namespace nx::media::hevc {

NX_CODEC_API std::vector<uint8_t> buildExtraDataAnnexB(const uint8_t* data, int32_t size);
NX_CODEC_API std::vector<uint8_t> buildExtraDataMp4(const std::vector<nal::NalUnitInfo>& nalUnits);
NX_CODEC_API std::vector<uint8_t> buildExtraDataMp4FromAnnexB(const uint8_t* data, int32_t size);

} // namespace nx::media::hevc
