// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <recording/stream_recorder_data.h>

#include <nx/streaming/media_data_packet.h>

#include <core/resource/resource_media_layout.h>
#include <core/resource/avi/avi_archive_metadata.h>

namespace nx {

/**
 * Recording 'physical-data' policy interface. It determines what needs to be done before data
 * saving process might be started and how this saving should be done exactly (for example to
 * local disks or to cloud).
 * StreamRecorder inherits this interface (but not implements). To make it work one should inherit
 * both from StreamRecorder and some concrete policy class which implements this interface.
 */
class AbstractRecordingContext
{
public:
    virtual ~AbstractRecordingContext() = default;
    virtual void closeRecordingContext(std::chrono::milliseconds durationMs) = 0;
    virtual void writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex) = 0;
    virtual AVMediaType streamAvMediaType(
        QnAbstractMediaData::DataType dataType, int streamIndex) const = 0;
    virtual int streamCount() const = 0;
    virtual bool doPrepareToStart(
        const QnConstAbstractMediaDataPtr& mediaData,
        const QnConstResourceVideoLayoutPtr& videoLayout,
        const AudioLayoutConstPtr& audioLayout,
        const QnAviArchiveMetadata& metadata) = 0;
    virtual std::optional<nx::recording::Error> getLastError() const = 0;
};

} // namespace nx