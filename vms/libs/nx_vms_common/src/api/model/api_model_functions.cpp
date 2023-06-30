// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <licensing/hardware_info.h>
#include <nx/fusion/model_functions.h>
#include <utils/email/email.h>

#include "audit/audit_record.h"
#include "backup_status_reply.h"
#include "camera_diagnostics_reply.h"
#include "camera_list_reply.h"
#include "configure_reply.h"
#include "getnonce_reply.h"
#include "hardware_ids_reply.h"
#include "manual_camera_seach_reply.h"
#include "ping_reply.h"
#include "recording_stats_reply.h"
#include "statistics_reply.h"
#include "system_settings_reply.h"
#include "test_email_settings_reply.h"
#include "time_reply.h"
#include "update_information_reply.h"
#include "virtual_camera_prepare_data.h"
#include "virtual_camera_prepare_reply.h"
#include "virtual_camera_reply.h"
#include "virtual_camera_status_reply.h"

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraDiagnosticsReply, (ubjson)(xml)(json)(csv_record), QnCameraDiagnosticsReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnManualResourceSearchEntry,
    (ubjson)(xml)(json)(csv_record), QnManualResourceSearchEntry_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnManualCameraSearchReply, (ubjson)(xml)(json)(csv_record), QnManualCameraSearchReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnStatisticsDataItem, (ubjson)(xml)(json)(csv_record), QnStatisticsDataItem_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnStatisticsReply, (ubjson)(xml)(json)(csv_record), QnStatisticsReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnTimeReply, (ubjson)(xml)(json)(csv_record), QnTimeReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    SyncTimeData, (ubjson)(xml)(json)(csv_record), SyncTimeData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ApiServerHardwareIdsData, (ubjson)(xml)(json)(csv_record), ApiServerHardwareIdsData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnTestEmailSettingsReply, (ubjson)(xml)(json)(csv_record), QnTestEmailSettingsReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPingReply, (ubjson)(xml)(json)(csv_record), QnPingReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnGetNonceReply, (ubjson)(xml)(json)(csv_record), QnGetNonceReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnSystemSettingsReply, (ubjson)(xml)(json)(csv_record), QnSystemSettingsReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraListReply, (ubjson)(xml)(json)(csv_record), QnCameraListReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnConfigureReply, (ubjson)(xml)(json)(csv_record), QnConfigureReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnUpdateFreeSpaceReply, (ubjson)(xml)(json)(csv_record), QnUpdateFreeSpaceReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCloudHostCheckReply, (ubjson)(xml)(json)(csv_record), QnCloudHostCheckReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnRecordingStatsData, (ubjson)(xml)(json)(csv_record), QnRecordingStatsData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCamRecordingStatsData, (ubjson)(xml)(json)(csv_record), QnCamRecordingStatsData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnBackupStatusData, (ubjson)(xml)(json)(csv_record), QnBackupStatusData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnVirtualCameraReply, (ubjson)(xml)(json)(csv_record), QnVirtualCameraReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnVirtualCameraStatusReply, (ubjson)(xml)(json)(csv_record), QnVirtualCameraStatusReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnVirtualCameraPrepareData, (ubjson)(xml)(json)(csv_record), QnVirtualCameraPrepareData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnVirtualCameraPrepareDataElement,
    (ubjson)(xml)(json)(csv_record), QnVirtualCameraPrepareDataElement_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnVirtualCameraPrepareReply, (ubjson)(xml)(json)(csv_record), QnVirtualCameraPrepareReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnVirtualCameraPrepareReplyElement,
    (ubjson)(xml)(json)(csv_record), QnVirtualCameraPrepareReplyElement_Fields)
