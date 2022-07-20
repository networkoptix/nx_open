// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/buffer_source.h>

namespace nx::network::http::test {

class NX_NETWORK_API CustomLengthBody:
    public BufferSource
{
    using base_type = BufferSource;

public:
    CustomLengthBody(
        std::string mimeType,
        nx::Buffer msgBody,
        std::optional<std::size_t> len = std::nullopt);

    virtual std::optional<uint64_t> contentLength() const override;

private:
    std::optional<std::size_t> m_len;
};

} // namespace nx::network::http::test
