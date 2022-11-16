// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/http_types.h>
#include <nx/network/rest/result.h>
#include <nx/utils/buffer.h>
#include <nx/utils/serialization/format.h>

namespace nx::vms::common::api {

NX_VMS_COMMON_API nx::network::rest::Result parseRestResult(
    nx::network::http::StatusCode::Value httpStatusCode,
    Qn::SerializationFormat format,
    nx::ConstBufferRefType messageBody);

} // namespace nx::vms::common::api
