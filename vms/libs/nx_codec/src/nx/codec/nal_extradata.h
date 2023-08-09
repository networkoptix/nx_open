// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <vector>

namespace nx::media::nal {

NX_CODEC_API std::vector<uint8_t> readH264SeqHeaderFromExtraData(const uint8_t* extraData, int extraDataSize);
NX_CODEC_API std::vector<uint8_t> readH265SeqHeaderFromExtraData(const uint8_t* extraData, int extraDataSize);

} // namespace nx::media::nal
