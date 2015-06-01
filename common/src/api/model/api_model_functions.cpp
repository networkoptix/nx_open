#include <utils/common/model_functions.h>
#include <utils/fusion/fusion_adaptor.h>

#include "camera_diagnostics_reply.h"
#include "manual_camera_seach_reply.h"
#include "storage_space_reply.h"
#include "storage_status_reply.h"
#include "time_reply.h"
#include "statistics_reply.h"
#include "connection_info.h"
#include "test_email_settings_reply.h"
#include "ping_reply.h"
#include "camera_list_reply.h"
#include "configure_reply.h"
#include "upload_update_reply.h"
#include "rebuild_archive_reply.h"

#define QN_MS_API_DATA_TYPES \
    (QnCameraDiagnosticsReply)\
    (QnManualCameraSearchStatus)\
    (QnManualCameraSearchSingleCamera)\
    (QnManualCameraSearchReply)\
    (QnStorageSpaceReply)\
    (QnStorageSpaceData)\
    (QnConnectionInfo)\
    (QnStorageStatusReply)\
    (QnStatisticsDataItem)\
    (QnStatisticsReply)\
    (QnTimeReply)\
    (QnStorageScanData)\
    (QnTestEmailSettingsReply)\
    (QnCompatibilityItem)\
    (QnPingReply)\
    (QnCameraListReply)\
    (QnConfigureReply) \
    (QnUploadUpdateReply)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(QN_MS_API_DATA_TYPES, (ubjson)(xml)(json)(csv_record), _Fields, (optional, true))

