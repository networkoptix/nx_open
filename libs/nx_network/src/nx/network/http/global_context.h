// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/fs/async_file_processor.h>
#include <nx/network/connection_cache.h>

namespace nx::network::http {

/**
 * The storage for global data related to HTTP implementation.
 */
class GlobalContext
{
public:
    nx::utils::fs::FileAsyncIoScheduler fileAsyncIoScheduler;
    nx::network::ConnectionCache connectionCache;
};

} // namespace nx::network::http
