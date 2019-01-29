#include "resource_types.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, ResourceStatus,
    (nx::vms::api::ResourceStatus::offline, "Offline")
    (nx::vms::api::ResourceStatus::unauthorized, "Unauthorized")
    (nx::vms::api::ResourceStatus::online, "Online")
    (nx::vms::api::ResourceStatus::recording, "Recording")
    (nx::vms::api::ResourceStatus::undefined, "NotDefined")
    (nx::vms::api::ResourceStatus::incompatible, "Incompatible"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::ResourceStatus, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, CameraStatusFlag,
    (nx::vms::api::CSF_NoFlags, "CSF_NoFlags")
    (nx::vms::api::CSF_HasIssuesFlag, "CSF_HasIssuesFlag"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::CameraStatusFlag, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, CameraStatusFlags,
    (nx::vms::api::CSF_NoFlags, "CSF_NoFlags")
    (nx::vms::api::CSF_HasIssuesFlag, "CSF_HasIssuesFlag"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::CameraStatusFlags, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, RecordingType,
    (nx::vms::api::RecordingType::always, "RT_Always")
    (nx::vms::api::RecordingType::motionOnly, "RT_MotionOnly")
    (nx::vms::api::RecordingType::never, "RT_Never")
    (nx::vms::api::RecordingType::motionAndLow, "RT_MotionAndLowQuality"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::RecordingType, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, StreamQuality,
    (nx::vms::api::StreamQuality::undefined, "undefined")
    (nx::vms::api::StreamQuality::undefined, "")
    (nx::vms::api::StreamQuality::lowest, "lowest")
    (nx::vms::api::StreamQuality::low, "low")
    (nx::vms::api::StreamQuality::normal, "normal")
    (nx::vms::api::StreamQuality::high, "high")
    (nx::vms::api::StreamQuality::highest, "highest")
    (nx::vms::api::StreamQuality::preset, "preset"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::StreamQuality, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, FailoverPriority,
    (nx::vms::api::FailoverPriority::never, "Never")
    (nx::vms::api::FailoverPriority::low, "Low")
    (nx::vms::api::FailoverPriority::medium, "Medium")
    (nx::vms::api::FailoverPriority::high, "High"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::FailoverPriority, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, CameraBackupQualities,
    (nx::vms::api::CameraBackup_Disabled, "CameraBackupDisabled")
    (nx::vms::api::CameraBackup_HighQuality, "CameraBackupHighQuality")
    (nx::vms::api::CameraBackup_LowQuality, "CameraBackupLowQuality")
    (nx::vms::api::CameraBackup_Both, "CameraBackupBoth")
    (nx::vms::api::CameraBackup_Default, "CameraBackupDefault"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::CameraBackupQualities, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, CameraBackupQuality,
    (nx::vms::api::CameraBackup_Disabled, "CameraBackupDisabled")
    (nx::vms::api::CameraBackup_HighQuality, "CameraBackupHighQuality")
    (nx::vms::api::CameraBackup_LowQuality, "CameraBackupLowQuality")
    (nx::vms::api::CameraBackup_Both, "CameraBackupBoth")
    (nx::vms::api::CameraBackup_Default, "CameraBackupDefault"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::CameraBackupQuality, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, IoModuleVisualStyle,
    (nx::vms::api::IoModuleVisualStyle::form, "Form")
    (nx::vms::api::IoModuleVisualStyle::tile, "Tile"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::IoModuleVisualStyle, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, ServerFlag,
    (nx::vms::api::SF_None, "SF_None")
    (nx::vms::api::SF_Edge, "SF_Edge")
    (nx::vms::api::SF_RemoteEC, "SF_RemoteEC")
    (nx::vms::api::SF_HasPublicIP, "SF_HasPublicIP")
    (nx::vms::api::SF_IfListCtrl, "SF_IfListCtrl")
    (nx::vms::api::SF_timeCtrl, "SF_timeCtrl")
    (nx::vms::api::SF_ArmServer, "SF_ArmServer")
    (nx::vms::api::SF_Has_HDD, "SF_Has_HDD")
    (nx::vms::api::SF_NewSystem, "SF_NewSystem")
    (nx::vms::api::SF_SupportsTranscoding, "SF_SupportsTranscoding")
    (nx::vms::api::SF_HasLiteClient, "SF_HasLiteClient")
    (nx::vms::api::SF_P2pSyncDone, "SF_P2pSyncDone"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::ServerFlag, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, ServerFlags,
(nx::vms::api::SF_None, "SF_None")
    (nx::vms::api::SF_Edge, "SF_Edge")
    (nx::vms::api::SF_RemoteEC, "SF_RemoteEC")
    (nx::vms::api::SF_HasPublicIP, "SF_HasPublicIP")
    (nx::vms::api::SF_IfListCtrl, "SF_IfListCtrl")
    (nx::vms::api::SF_timeCtrl, "SF_timeCtrl")
    (nx::vms::api::SF_ArmServer, "SF_ArmServer")
    (nx::vms::api::SF_Has_HDD, "SF_Has_HDD")
    (nx::vms::api::SF_NewSystem, "SF_NewSystem")
    (nx::vms::api::SF_SupportsTranscoding, "SF_SupportsTranscoding")
    (nx::vms::api::SF_HasLiteClient, "SF_HasLiteClient")
    (nx::vms::api::SF_P2pSyncDone, "SF_P2pSyncDone"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::ServerFlags, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, BackupType,
    (nx::vms::api::BackupType::manual, "BackupManual")
    (nx::vms::api::BackupType::realtime, "BackupRealTime")
    (nx::vms::api::BackupType::scheduled, "BackupSchedule"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::BackupType, (numeric)(debug))

#define StreamDataFilter_Values \
    (nx::vms::api::StreamDataFilter::media, "media") \
    (nx::vms::api::StreamDataFilter::motion, "motion") \
    (nx::vms::api::StreamDataFilter::objectDetection, "objects")

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, StreamDataFilter, StreamDataFilter_Values)
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, StreamDataFilters, StreamDataFilter_Values)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::StreamDataFilters, (debug))
