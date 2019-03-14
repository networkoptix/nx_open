#include "common_meta_types.h"

#include <atomic>

#include <QtCore/QMetaType>

#include <nx/network/socket_common.h>
#include <utils/common/request_param.h>
#include <utils/common/ldap.h>
#include <utils/common/optional.h>
#include <nx/fusion/serialization/json_functions.h>
#include <utils/math/space_mapper.h>
#include <nx/streaming/media_data_packet.h>
#include <core/resource/media_stream_capability.h>

#include <api/model/storage_space_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/camera_diagnostics_reply.h>
#include <api/model/manual_camera_seach_reply.h>
#include <api/model/servers_reply.h>
#include <api/model/kvpair.h>
#include <api/model/connection_info.h>
#include <api/model/time_reply.h>
#include <api/model/statistics_reply.h>
#include <api/model/rebuild_archive_reply.h>
#include <api/model/test_email_settings_reply.h>
#include <api/model/configure_reply.h>
#include <api/model/upload_update_reply.h>
#include <api/model/update_information_reply.h>
#include <api/model/backup_status_reply.h>
#include <api/model/getnonce_reply.h>
#include <api/model/wearable_camera_reply.h>
#include <api/model/wearable_status_reply.h>
#include <api/model/wearable_prepare_data.h>
#include <api/model/wearable_prepare_reply.h>
#include <api/runtime_info_manager.h>

#include <core/resource_access/resource_access_subject.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attributes.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/camera_history.h>
#include <core/resource/webpage_resource.h>

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>

#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource/videowall_matrix.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <recording/stream_recorder.h>
#include <recording/stream_recorder_data.h>

#include <core/misc/schedule_task.h>
#include <core/ptz/ptz_mapper.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/media_dewarping_params.h>

#include <nx/streaming/abstract_data_packet.h>

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/events/analytics_sdk_event.h>
#include <nx/vms/event/events/plugin_event.h>
#include <nx/vms/event/rule.h>

#include <licensing/license.h>

#include <network/system_description.h>
#include <network/networkoptixmodulerevealcommon.h>

#include <nx_ec/ec_api.h>

#include "api/model/api_ioport_data.h"
#include "api/model/recording_stats_reply.h"
#include "api/model/audit/audit_record.h"
#include "health/system_health.h"
#include <utils/common/credentials.h>
#include <core/resource/resource_data_structures.h>

#include <core/resource/camera_advanced_param.h>
#include <core/dataprovider/stream_mixer_data.h>

#include <nx/vms/common/p2p/downloader/file_information.h>
#include <nx/vms/api/metatypes.h>

#include <nx/core/ptz/override.h>

#include <nx/utils/metatypes.h>

QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::ResourceInfoLevel);

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
    qRegisterMetaType<Qn::ResourceStatus>();
    qRegisterMetaType<nx::vms::api::EventReason>();
    qRegisterMetaType<nx::vms::event::AnalyticsSdkEventPtr>();
    qRegisterMetaType<nx::vms::event::PluginEventPtr>();

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

    qRegisterMetaType<QnCameraUserAttributes>();
    qRegisterMetaType<QnCameraUserAttributesPtr>();
    qRegisterMetaType<QnCameraUserAttributesList>();

    qRegisterMetaType<QnMediaServerUserAttributes>();
    qRegisterMetaType<QnMediaServerUserAttributesPtr>();
    qRegisterMetaType<QnMediaServerUserAttributesList>();

    qRegisterMetaType<QnCameraBookmark>();
    qRegisterMetaType<QnCameraBookmarkList>();
    qRegisterMetaType<QnCameraBookmarkTags>("QnCameraBookmarkTags");
    /* The underlying type is identical to QStringList. */
    qRegisterMetaType<QnCameraBookmarkTag>();
    qRegisterMetaType<QnCameraBookmarkTagList>();

    qRegisterMetaType<QnLicensePtr>();
    qRegisterMetaType<QnLicenseList>();

    qRegisterMetaType<QnLayoutItemData>();
    qRegisterMetaType<QnVideoWallItem>();
    qRegisterMetaType<QnVideoWallPcData>();
    qRegisterMetaType<QnVideoWallControlMessage>();
    qRegisterMetaType<QnVideoWallMatrix>();

    qRegisterMetaType<QnMotionRegion>();
    qRegisterMetaType<QnScheduleTask>();
    qRegisterMetaType<QnScheduleTaskList>();

    qRegisterMetaType<QnRequestParamList>();
    qRegisterMetaType<QnRequestHeaderList>("QnRequestHeaderList");
    /* The underlying type is identical to QnRequestParamList. */
    qRegisterMetaType<QnReplyHeaderList>();
    qRegisterMetaType<QnHTTPRawResponse>();

    qRegisterMetaType<Qn::TimePeriodContent>();
    qRegisterMetaType<QnTimePeriodList>();
    qRegisterMetaType<MultiServerPeriodDataList>();

    qRegisterMetaType<TypeSpecificParamMap>();
    qRegisterMetaType<QnCameraAdvancedParamValue>();
    qRegisterMetaType<QnCameraAdvancedParamValueList>();

    qRegisterMetaType<QVector<int>>(); /* This one is used by QAbstractItemModel. */

    #ifdef ENABLE_DATA_PROVIDERS
    qRegisterMetaType<QnMetaDataV1Ptr>();
    qRegisterMetaType<StreamRecorderErrorStruct>();
    #endif

    qRegisterMetaType<nx::vms::event::AbstractActionPtr>();
    qRegisterMetaType<nx::vms::event::AbstractActionList>();
    qRegisterMetaType<nx::vms::event::AbstractEventPtr>();
    qRegisterMetaType<nx::vms::event::RulePtr>();
    qRegisterMetaType<nx::vms::event::RuleList>();

    qRegisterMetaType<QnAbstractDataPacketPtr>();
    qRegisterMetaType<QnConstAbstractDataPacketPtr>();

    qRegisterMetaType<QnStorageSpaceData>();
    qRegisterMetaType<QnStorageSpaceReply>();
    qRegisterMetaType<QnStorageStatusReply>();
    qRegisterMetaType<QnStatisticsReply>();
    qRegisterMetaType<ApiMultiserverServerDateTimeDataList>();
    qRegisterMetaType<QnTimeReply>();
    qRegisterMetaType<QnTestEmailSettingsReply>();
    qRegisterMetaType<QnCameraDiagnosticsReply>();
    qRegisterMetaType<QnStorageScanData>();
    qRegisterMetaType<QnBackupStatusData>();
    qRegisterMetaType<QnManualCameraSearchReply>();
    qRegisterMetaType<QnServersReply>();
    qRegisterMetaType<QnStatisticsData>();
    qRegisterMetaType<QnManualResourceSearchEntry>();
    qRegisterMetaType<QnWearableCameraReply>();
    qRegisterMetaType<QnWearableStatusReply>();
    qRegisterMetaType<QnWearablePrepareData>();
    qRegisterMetaType<QnWearablePrepareReply>();

    qRegisterMetaType<QnPtzPreset>();
    qRegisterMetaType<QnPtzPresetList>();
    qRegisterMetaType<QnPtzTour>();
    qRegisterMetaType<QnPtzTourList>();
    qRegisterMetaType<QnPtzData>();
    qRegisterMetaType<QnPtzLimits>();
    qRegisterMetaType<QnPtzMapperPtr>();
    qRegisterMetaType<Qn::PtzDataFields>();
    qRegisterMetaType<Qn::PtzCommand>();
    qRegisterMetaType<Ptz::Traits>();
    qRegisterMetaType<Ptz::Capabilities>();
    qRegisterMetaType<Qn::ResourceStatus>();
    qRegisterMetaType<Qn::StatusChangeReason>();

    qRegisterMetaType<QnIOPortData>();
    qRegisterMetaType<QnIOStateData>();
    qRegisterMetaType<QnAuditRecord>();
    qRegisterMetaType<QnAuditRecord*>();

    qRegisterMetaType<QnMediaDewarpingParams>();

    qRegisterMetaType<QnConnectionInfo>();
    qRegisterMetaType<Qn::PanicMode>();
    qRegisterMetaType<Qn::RebuildState>();

    qRegisterMetaType<Qn::ResourceInfoLevel>();
    qRegisterMetaTypeStreamOperators<Qn::ResourceInfoLevel>();

    qRegisterMetaType<QnGetNonceReply>();

    qRegisterMetaType<QnConfigureReply>();
    qRegisterMetaType<QnUploadUpdateReply>();
    qRegisterMetaType<QnUpdateFreeSpaceReply>();
    qRegisterMetaType<QnCloudHostCheckReply>();

    qRegisterMetaType<QnLdapUser>();
    qRegisterMetaType<QnLdapUsers>();

    qRegisterMetaType<Qn::ConnectionResult>();

    qRegisterMetaType<ec2::ErrorCode>("ErrorCode");
    qRegisterMetaType<ec2::NotificationSource>();
    qRegisterMetaType<ec2::AbstractECConnectionPtr>("AbstractECConnectionPtr");
    qRegisterMetaType<ec2::QnPeerTimeInfo>("QnPeerTimeInfo");
    qRegisterMetaType<ec2::QnPeerTimeInfoList>("QnPeerTimeInfoList");

    qRegisterMetaType<QnRecordingStatsReply>();
    qRegisterMetaType<QnAuditRecordList>();

    qRegisterMetaType<QnOptionalBool>();
    qRegisterMetaType<QnIOPortData>();
    qRegisterMetaType<QnIOPortDataList>();
    qRegisterMetaType<QList<QMap<QString, QString>>>();

    qRegisterMetaType<QList<nx::vms::common::Credentials>>();
    qRegisterMetaType<QnHttpConfigureRequestList>();
    qRegisterMetaType<QnBitrateList>();
    qRegisterMetaType<TwoWayAudioParams>();
    qRegisterMetaType<QnBounds>();
    qRegisterMetaType<QnCameraAdvancedParameterOverload>();

    qRegisterMetaType<QnSystemHealth::MessageType>("QnSystemHealth::MessageType");

    qRegisterMetaType<QnServerFields>();

    qRegisterMetaType<Qn::StatusChangeReason>("Qn::StatusChangeReason");
    qRegisterMetaType<nx::media::CameraTraits>();

    QnJsonSerializer::registerSerializer<QnPtzMapperPtr>();
    QnJsonSerializer::registerSerializer<Ptz::Traits>();
    QnJsonSerializer::registerSerializer<Ptz::Capabilities>();
    QnJsonSerializer::registerSerializer<QList<QMap<QString, QString>>>();

    QnJsonSerializer::registerSerializer<QnIOPortData>();
    QnJsonSerializer::registerSerializer<QnIOPortDataList>();
    QnJsonSerializer::registerSerializer<nx::vms::common::Credentials>();
    QnJsonSerializer::registerSerializer<QList<nx::vms::common::Credentials>>();
    QnJsonSerializer::registerSerializer<QnHttpConfigureRequestList>();
    QnJsonSerializer::registerSerializer<QnBitrateList>();
    QnJsonSerializer::registerSerializer<TwoWayAudioParams>();
    QnJsonSerializer::registerSerializer<QnBounds>();
    QnJsonSerializer::registerSerializer<std::vector<QString>>();
    QnJsonSerializer::registerSerializer<nx::core::ptz::Override>();

    QnJsonSerializer::registerSerializer<std::vector<QnCameraAdvancedParameterOverload>>();
    QnJsonSerializer::registerSerializer<nx::media::CameraTraits>();

    qRegisterMetaType<QnChannelMapping>();
    qRegisterMetaType<QList<QnChannelMapping>>();
    qRegisterMetaType<QnResourceChannelMapping>();
    qRegisterMetaType<QList<QnResourceChannelMapping>>();

    qRegisterMetaType<QnAbstractCompressedMetadataPtr>();

    qRegisterMetaType<nx::vms::common::p2p::downloader::FileInformation>();

    QnJsonSerializer::registerSerializer<QList<QnChannelMapping>>();
    QnJsonSerializer::registerSerializer<QList<QnResourceChannelMapping>>();

    qRegisterMetaType<std::chrono::hours>();
    qRegisterMetaType<std::chrono::minutes>();
    qRegisterMetaType<std::chrono::seconds>();
    qRegisterMetaType<std::chrono::milliseconds>();
    qRegisterMetaType<std::chrono::microseconds>();

    QMetaType::registerConverter<std::chrono::hours, std::chrono::minutes>();
    QMetaType::registerConverter<std::chrono::hours, std::chrono::seconds>();
    QMetaType::registerConverter<std::chrono::hours, std::chrono::milliseconds>();
    QMetaType::registerConverter<std::chrono::hours, std::chrono::microseconds>();
    QMetaType::registerConverter<std::chrono::minutes, std::chrono::seconds>();
    QMetaType::registerConverter<std::chrono::minutes, std::chrono::milliseconds>();
    QMetaType::registerConverter<std::chrono::minutes, std::chrono::microseconds>();
    QMetaType::registerConverter<std::chrono::seconds, std::chrono::milliseconds>();
    QMetaType::registerConverter<std::chrono::seconds, std::chrono::microseconds>();
    QMetaType::registerConverter<std::chrono::milliseconds, std::chrono::microseconds>();
}
