// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_msg_body_source.h"

#include "multipart_body_serializer.h"
#include "writable_message_body.h"

namespace nx::network::http {

/**
 * Used to generate and stream HTTP multipart message body.
 */
class NX_NETWORK_API MultipartMessageBodySource:
    public WritableMessageBody
{
    using base_type = WritableMessageBody;

public:
    MultipartMessageBodySource(std::string boundary);

    MultipartBodySerializer* serializer();

private:
    MultipartBodySerializer m_multipartBodySerializer;

    void onSomeDataAvailable(const ConstBufferRefType& data);
};

} // namespace nx::network::http
