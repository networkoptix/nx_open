// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <core/resource/resource_media_layout.h>
#include <recording/stream_recorder_data.h>
#include <utils/common/aspect_ratio.h>
#include <core/resource/avi/avi_archive_metadata.h>

#include <nx/vms/api/data/dewarping_data.h>

namespace nx {

/**
 * Callback interface for RecordingContext. It is implemented by StreamRecorder. Might be privately
 * inherited and called back by classes that implement AbstractRecordingContext.
 */
class AbstractRecordingContextCallback
{
public:
    virtual ~AbstractRecordingContextCallback() = default;
    virtual int64_t startTimeUs() const = 0;
    virtual bool isInterleavedStream() const = 0;
    virtual bool isUtcOffsetAllowed() const = 0;
};

} // namespace nx
