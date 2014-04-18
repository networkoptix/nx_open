#include <utils/serialization/json.h>

#include "camera_diagnostics_reply.h"
#include "manual_camera_seach_reply.h"
#include "storage_space_reply.h"
#include "storage_status_reply.h"
#include "time_reply.h"

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraDiagnosticsReply,         (json), (performedStep)(errorCode)(errorParams))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnManualCameraSearchStatus,       (json), (state)(current)(total))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnManualCameraSearchSingleCamera, (json), (name)(url)(manufacturer)(vendor)(existsInPool))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnManualCameraSearchReply,        (json), (status)(processUuid)(cameras))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStorageSpaceReply,              (json), (storages)(storageProtocols))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStorageSpaceData,               (json), (path)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isExternal)(isWritable)(isUsedForWriting))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStorageStatusReply,             (json), (pluginExists)(storage))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnTimeReply,                      (json), (utcTime)(timeZoneOffset))
