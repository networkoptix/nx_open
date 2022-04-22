// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <vector>

namespace nx::media::hevc {

NX_VMS_COMMON_API std::vector<uint8_t> buildExtraDataAnnexB(const uint8_t* data, int32_t size);
NX_VMS_COMMON_API std::vector<uint8_t> buildExtraDataMp4(const uint8_t* data, int32_t size);

} // namespace nx::media::hevc

