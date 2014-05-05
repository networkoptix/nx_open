#include <utils/serialization/json_functions.h>
#include <utils/serialization/binary_functions.h>
#include <utils/fusion/fusion_adaptor.h>

#include "camera_diagnostics_reply.h"
#include "manual_camera_seach_reply.h"
#include "storage_space_reply.h"
#include "storage_status_reply.h"
#include "time_reply.h"
#include "connection_info.h"

// TODO: #MSAPI Use a scheme similar to one in EC API: 
// 
// Define a _Fields macro where the struct/class definition is.
// Then generate all functions using a single QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES invocation.

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraDiagnosticsReply,          (json)(binary), (performedStep)(errorCode)(errorParams))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnManualCameraSearchStatus,        (json)(binary), (state)(current)(total))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnManualCameraSearchSingleCamera,  (json)(binary), (name)(url)(manufacturer)(vendor)(existsInPool))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnManualCameraSearchReply,         (json)(binary), (status)(processUuid)(cameras))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStorageSpaceReply,               (json)(binary), (storages)(storageProtocols))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStorageSpaceData,                (json)(binary), (path)(storageId)(totalSpace)(freeSpace)(reservedSpace)(isExternal)(isWritable)(isUsedForWriting))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStorageStatusReply,              (json)(binary), (pluginExists)(storage))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnTimeReply,                       (json)(binary), (utcTime)(timeZoneOffset))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnConnectionInfo,                  (json)(binary), (ecUrl)(version)(compatibilityItems)(proxyPort)(ecsGuid)(publicIp)(brand))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCompatibilityItem,               (json)(binary), QnCompatibilityItem_Fields)
