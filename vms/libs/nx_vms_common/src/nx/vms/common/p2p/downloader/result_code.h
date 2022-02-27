// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::common::p2p::downloader {

NX_REFLECTION_ENUM_CLASS(ResultCode,
    ok,
    loadingDownloads,
    ioError,
    fileDoesNotExist,
    fileAlreadyExists,
    fileAlreadyDownloaded,
    invalidChecksum,
    invalidFileSize,
    invalidChunkIndex,
    invalidChunkSize,
    noFreeSpace
)

} // namespace nx::vms::common::p2p::downloader
