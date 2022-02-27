// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base64_decoder_filter.h"

#include <nx/utils/string.h>

namespace nx::network::http {

bool Base64DecoderFilter::processData(const nx::ConstBufferRefType& data)
{
    return m_nextFilter->processData(nx::utils::fromBase64(data));
}

size_t Base64DecoderFilter::flush()
{
    return 0;
}

} // namespace nx::network::http
