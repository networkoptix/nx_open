#pragma once

#include <core/resource/resource_fwd.h>

struct AVFormatContext;

enum class StreamRecorderRole
{
    serverRecording,
    fileExport,
    fileExportWithEmptyContext
};

enum class StreamRecorderError
{
    noError,
    containerNotFound,
    fileCreate,
    videoStreamAllocation,
    audioStreamAllocation,
    invalidAudioCodec,
    incompatibleCodec,
    fileWrite,
    invalidResourceType,
    dataNotFound,

    LastError
};

struct StreamRecorderErrorStruct
{
    StreamRecorderError lastError;
    QnStorageResourcePtr storage;

    StreamRecorderErrorStruct();

    StreamRecorderErrorStruct(StreamRecorderError lastError, const QnStorageResourcePtr& storage);
};

Q_DECLARE_METATYPE(StreamRecorderErrorStruct)
