#include <utils/common/model_functions.h>
#include <utils/fusion/fusion_adaptor.h>

#include "camera_diagnostics_reply.h"
#include "manual_camera_seach_reply.h"
#include "storage_space_reply.h"
#include "storage_status_reply.h"
#include "time_reply.h"
#include "connection_info.h"

#define QN_MS_API_DATA_TYPES \
    (QnCameraDiagnosticsReply)\
    (QnManualCameraSearchStatus)\
    (QnManualCameraSearchSingleCamera)\
    (QnManualCameraSearchReply)\
    (QnStorageSpaceReply)\
    (QnStorageSpaceData)\
    (QnStorageStatusReply)\
    (QnTimeReply)\
    (QnConnectionInfo)\
    (QnCompatibilityItem)\

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(QN_MS_API_DATA_TYPES, (json)(binary)(csv_record), _Fields)
