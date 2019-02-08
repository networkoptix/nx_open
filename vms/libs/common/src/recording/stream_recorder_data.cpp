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
