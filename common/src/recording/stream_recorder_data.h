#pragma once

#include <core/resource/resource_fwd.h>

struct AVFormatContext;

enum class StreamRecorderRole
{
    Role_ServerRecording,
    Role_FileExport,
    Role_FileExportWithEmptyContext
};

enum class StreamRecorderError
{
    NoError,
    ContainerNotFoundError,
    FileCreateError,
    VideoStreamAllocationError,
    AudioStreamAllocationError,
    InvalidAudioCodecError,
    IncompatibleCodecError,
    FileWriteError,
    InvalidResourceType,
    LastError
};

struct StreamRecorderErrorStruct
{
    StreamRecorderError lastError;
    QnStorageResourcePtr storage;

    StreamRecorderErrorStruct();

    StreamRecorderErrorStruct(StreamRecorderError lastError, const QnStorageResourcePtr& storage);
};

struct StreamRecorderContext
{
    QString fileName;
    AVFormatContext* formatCtx;
    QnStorageResourcePtr storage;
    qint64 totalWriteTimeNs;

    StreamRecorderContext(const QString& fileName,
        const QnStorageResourcePtr& storage);
};

Q_DECLARE_METATYPE(StreamRecorderErrorStruct)
