#include <nx/fusion/model_functions.h>
#include <nx/fusion/fusion/fusion_adaptor.h>

#include "camera_diagnostics_reply.h"
#include "manual_camera_seach_reply.h"
#include "storage_space_reply.h"
#include "storage_status_reply.h"
#include "time_reply.h"
#include "hardware_ids_reply.h"
#include "statistics_reply.h"
#include "connection_info.h"
#include "test_email_settings_reply.h"
#include "ping_reply.h"
#include "getnonce_reply.h"
#include "system_settings_reply.h"
#include "recording_stats_reply.h"
#include "camera_list_reply.h"
#include "configure_reply.h"
#include "upload_update_reply.h"
#include "update_information_reply.h"
#include "rebuild_archive_reply.h"
#include "api_ioport_data.h"
#include "audit/audit_record.h"
#include "licensing/hardware_info.h"
#include "backup_status_reply.h"
#include "wearable_camera_reply.h"
#include <utils/email/email.h>

#define QN_MS_API_DATA_TYPES \
    (QnCameraDiagnosticsReply)\
    (QnManualResourceSearchStatus)\
    (QnManualResourceSearchEntry)\
    (QnManualCameraSearchReply)\
    (QnStorageSpaceReply)\
    (QnConnectionInfo)\
    (QnStorageStatusReply)\
    (QnStatisticsDataItem)\
    (QnStatisticsReply)\
    (QnTimeReply)\
    (ApiServerDateTimeData)\
    (ApiServerHardwareIdsData)\
    (QnTestEmailSettingsReply)\
    (QnCompatibilityItem)\
    (QnPingReply)\
    (QnGetNonceReply)\
    (QnSystemSettingsReply)\
    (QnCameraListReply)\
    (QnConfigureReply) \
    (QnUploadUpdateReply)\
    (QnUpdateFreeSpaceReply)\
    (QnCloudHostCheckReply)\
    (QnRecordingStatsData)\
    (QnCamRecordingStatsData)

#define QN_MS_API_COMPARABLE_DATA_TYPES \
    (QnStorageSpaceData)\
    (QnStorageScanData)\
    (QnBackupStatusData)\
    (QnWearableCameraReply)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_MS_API_DATA_TYPES, (ubjson)(xml)(json)(csv_record), _Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnIOPortData)(QnCameraPortsData)(QnIOStateData)(QnCameraIOStateData),
    (eq)(ubjson)(json),
    _Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnAuditRecord), (ubjson)(xml)(json)(csv_record)(eq)(sql_record), _Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnAuthSession), (ubjson)(xml)(json)(csv_record)(eq)(sql_record), _Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_MS_API_COMPARABLE_DATA_TYPES, (ubjson)(xml)(json)(csv_record)(eq), _Fields)
