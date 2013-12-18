#include <utils/common/json.h>

#include "camera_diagnostics_reply.h"
#include "manual_camera_seach_reply.h"
#include "storage_space_reply.h"
#include "storage_status_reply.h"
#include "time_reply.h"

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnCameraDiagnosticsReply, (performedStep)(errorCode)(errorParams))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchStatus, (state)(current)(total))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchSingleCamera, (name)(url)(manufacturer)(existsInPool))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchReply, (status)(processUuid)(cameras))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnStorageSpaceReply, (storages)(storageProtocols))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnStorageSpaceData, (path)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isExternal)(isWritable)(isUsedForWriting))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnStorageStatusReply, (pluginExists)(storage))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnTimeReply, (utcTime)(timeZoneOffset))
