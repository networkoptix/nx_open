// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_meta_types.h"

#include <atomic>

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include <api/model/api_ioport_data.h>
#include <api/model/audit/audit_record.h>
#include <api/model/backup_status_reply.h>
#include <api/model/camera_diagnostics_reply.h>
#include <api/model/configure_reply.h>
#include <api/model/getnonce_reply.h>
#include <api/model/manual_camera_seach_reply.h>
#include <api/model/recording_stats_reply.h>
#include <api/model/statistics_reply.h>
#include <api/model/test_email_settings_reply.h>
#include <api/model/time_reply.h>
#include <api/model/update_information_reply.h>
#include <api/model/virtual_camera_prepare_data.h>
#include <api/model/virtual_camera_prepare_reply.h>
#include <api/model/virtual_camera_reply.h>
#include <api/model/virtual_camera_status_reply.h>
#include <api/runtime_info_manager.h>
#include <core/dataprovider/stream_mixer_data.h>
#include <core/misc/schedule_task.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/ptz_mapper.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource/resource.h>
#include <core/resource/resource_data_structures.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_matrix.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_map.h>
#include <core/resource_access/resource_access_subject.h>
#include <licensing/license.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/media/abstract_data_packet.h>
#include <nx/media/media_data_packet.h>
#include <nx/network/rest/params.h>
#include <nx/network/socket_common.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/string.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/media_stream_capability.h>
#include <nx/vms/api/metatypes.h>
#include <nx/vms/common/p2p/downloader/file_information.h>
#include <nx/vms/common/ptz/command.h>
#include <nx/vms/common/ptz/datafield.h>
#include <nx/vms/common/ptz/override.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/events/analytics_sdk_event.h>
#include <nx/vms/event/events/analytics_sdk_object_detected.h>
#include <nx/vms/event/events/fan_error_event.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>
#include <nx/vms/event/events/poe_over_budget_event.h>
#include <nx/vms/event/events/server_certificate_error.h>
#include <nx/vms/event/rule.h>
#include <nx_ec/ec_api_common.h>
#include <nx_ec/ec_api_fwd.h>
#include <recording/stream_recorder.h>
#include <recording/stream_recorder_data.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <utils/common/optional.h>
#include <utils/math/space_mapper.h>

using namespace nx::vms::common;

void QnCommonMetaTypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    nx::utils::Metatypes::initialize();
    nx::vms::api::Metatypes::initialize();

    qRegisterMetaType<uintptr_t>("uintptr_t");

    qRegisterMetaType<QHostAddress>();
    qRegisterMetaType<QAuthenticator>();
    qRegisterMetaType<Qt::ConnectionType>();
    qRegisterMetaType<Qt::Orientations>();

    qRegisterMetaType<QnPeerRuntimeInfo>();
    qRegisterMetaType<nx::network::HostAddress>();
    qRegisterMetaType<nx::network::SocketAddress>();

    //qRegisterMetaType<QnParam>();

    qRegisterMetaType<QnResourceTypeList>();
    qRegisterMetaType<QnResourcePtr>();
    qRegisterMetaType<QnResourceList>();
    qRegisterMetaType<Qn::ResourceFlags>();
    QMetaType::registerConverter<Qn::ResourceFlags, int>();

    qRegisterMetaType<nx::vms::event::EventReason>("nx::vms::event::EventReason");
    qRegisterMetaType<nx::vms::event::PluginDiagnosticEventPtr>(
        "nx::vms::event::PluginDiagnosticEventPtr");
    qRegisterMetaType<nx::vms::event::PoeOverBudgetEventPtr>(
        "nx::vms::event::PoeOverBudgetEventPtr");
    qRegisterMetaType<nx::vms::event::FanErrorEventPtr>(
        "nx::vms::event::FanErrorEventPtr");

    qRegisterMetaType<QnUserResourcePtr>();
    qRegisterMetaType<QnLayoutResourcePtr>();
    qRegisterMetaType<QnMediaServerResourcePtr>();
    qRegisterMetaType<QnVirtualCameraResourcePtr>();
    qRegisterMetaType<QnVirtualCameraResourceList>();
    qRegisterMetaType<QnSecurityCamResourcePtr>();
    qRegisterMetaType<QnStorageResourcePtr>();
    qRegisterMetaType<QnVideoWallResourcePtr>();

    qRegisterMetaType<QnUserResourceList>();
    qRegisterMetaType<QnVideoWallResourceList>();
    qRegisterMetaType<QnWebPageResourcePtr>();
    qRegisterMetaType<QnWebPageResourceList>();

    qRegisterMetaType<nx::vms::common::AnalyticsPluginResourcePtr>();
    qRegisterMetaType<nx::vms::common::AnalyticsEngineResourcePtr>();

    qRegisterMetaType<QnResourceAccessSubject>();

    qRegisterMetaType<QnCameraBookmark>();
    qRegisterMetaType<QnCameraBookmarkList>();
    qRegisterMetaType<QnCameraBookmarkTags>("QnCameraBookmarkTags");
    /* The underlying type is identical to QStringList. */
    qRegisterMetaType<QnCameraBookmarkTag>();
    qRegisterMetaType<QnCameraBookmarkTagList>();

    qRegisterMetaType<QnLicensePtr>();
    qRegisterMetaType<QnLicenseList>();

    qRegisterMetaType<LayoutItemData>();
    qRegisterMetaType<QnVideoWallItem>();
    qRegisterMetaType<QnVideoWallPcData>();
    qRegisterMetaType<QnVideoWallControlMessage>();
    qRegisterMetaType<QnVideoWallMatrix>();

    qRegisterMetaType<QnMotionRegion>();
    qRegisterMetaType<QnScheduleTask>();
    qRegisterMetaType<QnScheduleTaskList>();

    qRegisterMetaType<nx::network::rest::Params>();

    qRegisterMetaType<Qn::TimePeriodContent>();
    qRegisterMetaType<QnTimePeriodList>();
    qRegisterMetaType<MultiServerPeriodDataList>();

    qRegisterMetaType<QnCameraAdvancedParams>();
    qRegisterMetaType<QnCameraAdvancedParamValue>();
    qRegisterMetaType<QnCameraAdvancedParamValueList>();

    qRegisterMetaType<QVector<int>>(); /* This one is used by QAbstractItemModel. */

    qRegisterMetaType<QnMetaDataV1Ptr>();
    qRegisterMetaType<std::optional<nx::recording::Error>>();

    qRegisterMetaType<nx::vms::event::AbstractActionPtr>();
    qRegisterMetaType<nx::vms::event::AbstractActionList>();
    qRegisterMetaType<nx::vms::event::AbstractEventPtr>();
    qRegisterMetaType<nx::vms::event::RulePtr>();
    qRegisterMetaType<nx::vms::event::RuleList>();

    qRegisterMetaType<QnAbstractDataPacketPtr>();
    qRegisterMetaType<QnConstAbstractDataPacketPtr>();

    // Required for queuing arguments of type 'QnArchiveStreamReader*' in QObject::connect.
    qRegisterMetaType<QnArchiveStreamReader>();

    qRegisterMetaType<QnStatisticsReply>();
    qRegisterMetaType<QnTimeReply>();
    qRegisterMetaType<QnTestEmailSettingsReply>();
    qRegisterMetaType<QnCameraDiagnosticsReply>();
    qRegisterMetaType<QnBackupStatusData>();
    qRegisterMetaType<QnManualCameraSearchReply>();
    qRegisterMetaType<QnStatisticsData>();
    qRegisterMetaType<QnManualResourceSearchEntry>();
    qRegisterMetaType<QnVirtualCameraReply>();
    qRegisterMetaType<QnVirtualCameraStatusReply>();
    qRegisterMetaType<QnVirtualCameraPrepareData>();
    qRegisterMetaType<QnVirtualCameraPrepareReply>();

    qRegisterMetaType<QnPtzPreset>();
    qRegisterMetaType<QnPtzPresetList>();
    qRegisterMetaType<QnPtzTour>();
    qRegisterMetaType<QnPtzTourList>();
    qRegisterMetaType<QnPtzData>();
    qRegisterMetaType<QnPtzLimits>();
    qRegisterMetaType<QnPtzMapperPtr>();
    qRegisterMetaType<nx::vms::common::ptz::Command>();
    qRegisterMetaType<nx::vms::common::ptz::DataFields>();
    qRegisterMetaType<Ptz::Traits>();
    qRegisterMetaType<Ptz::Capabilities>();
    qRegisterMetaType<Qn::StatusChangeReason>();

    qRegisterMetaType<QnIOPortData>();
    qRegisterMetaType<QnIOStateData>();
    qRegisterMetaType<QnAuditRecord>();
    qRegisterMetaType<QnAuditRecord*>();

    qRegisterMetaType<Qn::PanicMode>();

    qRegisterMetaType<Qn::ResourceInfoLevel>();

    qRegisterMetaType<QnGetNonceReply>();

    qRegisterMetaType<QnConfigureReply>();
    qRegisterMetaType<QnUpdateFreeSpaceReply>();
    qRegisterMetaType<QnCloudHostCheckReply>();

    qRegisterMetaType<ec2::ErrorCode>("ErrorCode");
    qRegisterMetaType<ec2::NotificationSource>();
    qRegisterMetaType<ec2::AbstractECConnectionPtr>("AbstractECConnectionPtr");

    qRegisterMetaType<QnRecordingStatsReply>();
    qRegisterMetaType<QnAuditRecordList>();

    qRegisterMetaType<QnOptionalBool>();
    qRegisterMetaType<QnIOPortData>();
    qRegisterMetaType<QnIOPortDataList>();
    qRegisterMetaType<QList<QMap<QString, QString>>>();

    qRegisterMetaType<QnHttpConfigureRequestList>();
    qRegisterMetaType<QnBitrateList>();
    qRegisterMetaType<TwoWayAudioParams>();
    qRegisterMetaType<QnBounds>();
    qRegisterMetaType<QnCameraAdvancedParameterOverload>();

    qRegisterMetaType<system_health::MessageType>();

    qRegisterMetaType<Qn::StatusChangeReason>("Qn::StatusChangeReason");
    qRegisterMetaType<nx::vms::api::CameraTraits>();

    qRegisterMetaType<std::map<QnUuid, nx::vms::api::AccessRights>>();
    qRegisterMetaType<nx::core::access::ResourceAccessMap>();

    QnJsonSerializer::registerSerializer<QnPtzMapperPtr>();
    QnJsonSerializer::registerSerializer<Ptz::Traits>();
    QnJsonSerializer::registerSerializer<Ptz::Capabilities>();
    QnJsonSerializer::registerSerializer<QList<QMap<QString, QString>>>();

    QnJsonSerializer::registerSerializer<QnIOPortData>();
    QnJsonSerializer::registerSerializer<QnIOPortDataList>();
    QnJsonSerializer::registerSerializer<QnHttpConfigureRequestList>();
    QnJsonSerializer::registerSerializer<QnBitrateList>();
    QnJsonSerializer::registerSerializer<TwoWayAudioParams>();
    QnJsonSerializer::registerSerializer<QnBounds>();
    QnJsonSerializer::registerSerializer<std::vector<QString>>();
    QnJsonSerializer::registerSerializer<nx::vms::common::ptz::Override>();

    QnJsonSerializer::registerSerializer<std::vector<QnCameraAdvancedParameterOverload>>();
    QnJsonSerializer::registerSerializer<nx::vms::api::CameraTraits>();

    qRegisterMetaType<QnChannelMapping>();
    qRegisterMetaType<QList<QnChannelMapping>>();
    qRegisterMetaType<QnResourceChannelMapping>();
    qRegisterMetaType<QList<QnResourceChannelMapping>>();

    qRegisterMetaType<QnAbstractCompressedMetadataPtr>();

    qRegisterMetaType<nx::vms::common::p2p::downloader::FileInformation>();

    QnJsonSerializer::registerSerializer<QList<QnChannelMapping>>();
    QnJsonSerializer::registerSerializer<QList<QnResourceChannelMapping>>();

    qRegisterMetaType<nx::String>();

}
