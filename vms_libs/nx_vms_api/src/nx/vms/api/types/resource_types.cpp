#include "resource_types.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::ResourceStatus, (numeric))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, ResourceStatus,
    (nx::vms::api::ResourceStatus::offline, "Offline")
    (nx::vms::api::ResourceStatus::unauthorized, "Unauthorized")
    (nx::vms::api::ResourceStatus::online, "Online")
    (nx::vms::api::ResourceStatus::recording, "Recording")
    (nx::vms::api::ResourceStatus::undefined, "NotDefined")
    (nx::vms::api::ResourceStatus::incompatible, "Incompatible"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, CameraStatusFlag,
    (nx::vms::api::CSF_NoFlags, "CSF_NoFlags")
    (nx::vms::api::CSF_HasIssuesFlag, "CSF_HasIssuesFlag"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, CameraStatusFlags,
    (nx::vms::api::CSF_NoFlags, "CSF_NoFlags")
    (nx::vms::api::CSF_HasIssuesFlag, "CSF_HasIssuesFlag"))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::RecordingType, (numeric))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, RecordingType,
    (nx::vms::api::RecordingType::always, "RT_Always")
    (nx::vms::api::RecordingType::motionOnly, "RT_MotionOnly")
    (nx::vms::api::RecordingType::never, "RT_Never")
    (nx::vms::api::RecordingType::motionAndLow, "RT_MotionAndLowQuality"))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::StreamQuality, (numeric))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, StreamQuality,
    (nx::vms::api::StreamQuality::undefined, "")
    (nx::vms::api::StreamQuality::lowest, "lowest")
    (nx::vms::api::StreamQuality::low, "low")
    (nx::vms::api::StreamQuality::normal, "normal")
    (nx::vms::api::StreamQuality::high, "high")
    (nx::vms::api::StreamQuality::highest, "highest")
    (nx::vms::api::StreamQuality::preset, "preset"))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::FailoverPriority, (numeric))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, FailoverPriority,
    (nx::vms::api::FailoverPriority::never, "Never")
    (nx::vms::api::FailoverPriority::low, "Low")
    (nx::vms::api::FailoverPriority::medium, "Medium")
    (nx::vms::api::FailoverPriority::high, "High"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, CameraBackupQualities,
    (nx::vms::api::CameraBackup_Disabled, "CameraBackupDisabled")
    (nx::vms::api::CameraBackup_HighQuality, "CameraBackupHighQuality")
    (nx::vms::api::CameraBackup_LowQuality, "CameraBackupLowQuality")
    (nx::vms::api::CameraBackup_Both, "CameraBackupBoth")
    (nx::vms::api::CameraBackup_Default, "CameraBackupDefault"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, CameraBackupQuality,
    (nx::vms::api::CameraBackup_Disabled, "CameraBackupDisabled")
    (nx::vms::api::CameraBackup_HighQuality, "CameraBackupHighQuality")
    (nx::vms::api::CameraBackup_LowQuality, "CameraBackupLowQuality")
    (nx::vms::api::CameraBackup_Both, "CameraBackupBoth")
    (nx::vms::api::CameraBackup_Default, "CameraBackupDefault"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, IoModuleVisualStyle,
    (nx::vms::api::IoModuleVisualStyle::form, "Form")
    (nx::vms::api::IoModuleVisualStyle::tile, "Tile"))
