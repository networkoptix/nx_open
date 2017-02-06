
#include "stream_recorder_data.h"

StreamRecorderErrorStruct::StreamRecorderErrorStruct() :
    lastError(StreamRecorderError::noError),
    storage(QnStorageResourcePtr())
{}

StreamRecorderErrorStruct::StreamRecorderErrorStruct(StreamRecorderError lastError,
    const QnStorageResourcePtr& storage)
    :
    lastError(lastError),
    storage(storage)
{
}

StreamRecorderContext::StreamRecorderContext(const QString& fileName,
    const QnStorageResourcePtr& storage)
    :
    fileName(fileName),
    formatCtx(nullptr),
    storage(storage),
    totalWriteTimeNs(0)
{
}

