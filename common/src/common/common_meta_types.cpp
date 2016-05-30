#include "common_meta_types.h"

#include <QtCore/QMetaType>

#include <nx/network/mac_address.h>
#include <nx/network/socket_common.h>
#include <utils/common/request_param.h>
#include <nx/utils/uuid.h>
#include <utils/common/ldap.h>
#include <utils/common/optional.h>
#include <utils/serialization/json_functions.h>
#include <utils/math/space_mapper.h>

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
#include <api/model/backup_status_reply.h>
#include <api/runtime_info_manager.h>

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

#include <core/misc/schedule_task.h>
#include <core/ptz/ptz_mapper.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/item_dewarping_params.h>

#include <nx/streaming/abstract_data_packet.h>

#include <core/onvif/onvif_config_data.h>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_event_rule.h>
#include <business/business_fwd.h>

#include <licensing/license.h>

#include <network/multicast_module_finder.h>
#include <network/system_description.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_lock_data.h>
#include <nx_ec/data/api_discovery_data.h>
#include <nx_ec/data/api_resource_data.h>
#include <nx_ec/data/api_reverse_connection_data.h>
#include <nx_ec/data/api_full_info_data.h>
#include <nx_ec/data/api_webpage_data.h>
#include <nx_ec/data/api_videowall_data.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/data/api_resource_type_data.h>
#include <nx_ec/data/api_license_data.h>
#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/data/api_camera_data.h>
#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_access_rights_data.h>

#include "api/model/api_ioport_data.h"
#include "api/model/recording_stats_reply.h"
#include "api/model/audit/audit_record.h"
#include "health/system_health.h"

namespace {
    volatile bool qn_commonMetaTypes_initialized = false;
}

QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::Corner)
QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::ResourceInfoLevel);

void QnCommonMetaTypes::initialize() {
    /* Note that running the code twice is perfectly OK,
     * so we don't need heavyweight synchronization here. */
    if(qn_commonMetaTypes_initialized)
        return;

    qRegisterMetaType<QnUuid>();
    qRegisterMetaType<QSet<QnUuid>>("QSet<QnUuid>");
    qRegisterMetaType<QHostAddress>();
    qRegisterMetaType<QAuthenticator>();
    qRegisterMetaType<Qt::ConnectionType>();
    qRegisterMetaType<Qt::Orientations>();

    qRegisterMetaType<QnMacAddress>();
    qRegisterMetaType<QnPeerRuntimeInfo>();
    qRegisterMetaType<SocketAddress>();

    //qRegisterMetaType<QnParam>();

    qRegisterMetaType<QnResourceTypeList>();
    qRegisterMetaType<QnResourcePtr>();
    qRegisterMetaType<QnResourceList>();
    qRegisterMetaType<Qn::ResourceStatus>();
    qRegisterMetaType<Qn::BitratePerGopType>();
    qRegisterMetaType<QnBusiness::EventReason>();

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

    qRegisterMetaType<QnCameraUserAttributes>();
    qRegisterMetaType<QnCameraUserAttributesPtr>();
    qRegisterMetaType<QnCameraUserAttributesList>();

    qRegisterMetaType<QnMediaServerUserAttributes>();
    qRegisterMetaType<QnMediaServerUserAttributesPtr>();
    qRegisterMetaType<QnMediaServerUserAttributesList>();

    qRegisterMetaType<QnCameraBookmark>();
    qRegisterMetaType<QnCameraBookmarkList>();
    qRegisterMetaType<QnCameraBookmarkTags>("QnCameraBookmarkTags");/* The underlying type is identical to QStringList. */
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
    qRegisterMetaType<QnRequestHeaderList>("QnRequestHeaderList"); /* The underlying type is identical to QnRequestParamList. */
    qRegisterMetaType<QnReplyHeaderList>();
    qRegisterMetaType<QnHTTPRawResponse>();

    qRegisterMetaType<Qn::TimePeriodContent>();
    qRegisterMetaType<QnTimePeriodList>();
    qRegisterMetaType<MultiServerPeriodDataList>();

    qRegisterMetaType<QnSoftwareVersion>();
    qRegisterMetaTypeStreamOperators<QnSoftwareVersion>();
    qRegisterMetaType<QnSystemInformation>();
    qRegisterMetaTypeStreamOperators<QnSystemInformation>();

    qRegisterMetaType<TypeSpecificParamMap>();
    qRegisterMetaType<QnCameraAdvancedParamValue>();
	qRegisterMetaType<QnCameraAdvancedParamValueList>();

    qRegisterMetaType<QVector<int> >(); /* This one is used by QAbstractItemModel. */

#ifdef ENABLE_DATA_PROVIDERS
    qRegisterMetaType<QnMetaDataV1Ptr>();
    qRegisterMetaType<QnStreamRecorder::ErrorStruct>();
#endif

    qRegisterMetaType<QnAbstractBusinessActionPtr>();
    qRegisterMetaType<QnAbstractBusinessActionList>();
    qRegisterMetaType<QnBusinessActionDataListPtr>();
    qRegisterMetaType<QnAbstractBusinessEventPtr>();
    qRegisterMetaType<QnBusinessEventRulePtr>();
    qRegisterMetaType<QnBusinessEventRuleList>();
    qRegisterMetaType<QnAbstractDataPacketPtr>();
    qRegisterMetaType<QnConstAbstractDataPacketPtr>();

    qRegisterMetaType<QnStorageSpaceData>();
    qRegisterMetaType<QnStorageSpaceReply>();
    qRegisterMetaType<QnStorageStatusReply>();
    qRegisterMetaType<QnStatisticsReply>();
    qRegisterMetaType<QnTimeReply>();
    qRegisterMetaType<QnTestEmailSettingsReply>();
    qRegisterMetaType<QnCameraDiagnosticsReply>();
    qRegisterMetaType<QnStorageScanData>();
    qRegisterMetaType<QnBackupStatusData>();
    qRegisterMetaType<QnManualCameraSearchReply>();
    qRegisterMetaType<QnServersReply>();
    qRegisterMetaType<QnStatisticsData>();
    qRegisterMetaType<QnManualResourceSearchEntry>();

    qRegisterMetaType<QnPtzPreset>();
    qRegisterMetaType<QnPtzPresetList>();
    qRegisterMetaType<QnPtzTour>();
    qRegisterMetaType<QnPtzTourList>();
    qRegisterMetaType<QnPtzData>();
    qRegisterMetaType<QnPtzLimits>();
    qRegisterMetaType<QnPtzMapperPtr>();
    qRegisterMetaType<Qn::PtzDataFields>();
    qRegisterMetaType<Qn::PtzCommand>();
    qRegisterMetaType<Qn::PtzTraits>();
    qRegisterMetaType<Qn::PtzCapabilities>();

    qRegisterMetaType<QnOnvifConfigDataPtr>();

    qRegisterMetaType<QnIOPortData>();
    qRegisterMetaType<QnIOStateData>();
    qRegisterMetaType<QnAuditRecord>();
    qRegisterMetaType<QnAuditRecord*>();

    qRegisterMetaType<QnMediaDewarpingParams>();
    qRegisterMetaType<QnItemDewarpingParams>();

    qRegisterMetaType<Qn::Corner>();
    qRegisterMetaTypeStreamOperators<Qn::Corner>();

    qRegisterMetaType<QnConnectionInfo>();
    qRegisterMetaType<Qn::PanicMode>();
    qRegisterMetaType<Qn::RebuildState>();

    qRegisterMetaType<Qn::ResourceInfoLevel>();
    qRegisterMetaTypeStreamOperators<Qn::ResourceInfoLevel>();

    qRegisterMetaType<QnModuleInformation>();
    qRegisterMetaType<QnModuleInformationWithAddresses>();
    qRegisterMetaType<QList<QnModuleInformation>>();
    qRegisterMetaType<QList<QnModuleInformationWithAddresses>>();

    qRegisterMetaType<QnConfigureReply>();
    qRegisterMetaType<QnUploadUpdateReply>();

    qRegisterMetaType<QnLdapUser>();
    qRegisterMetaType<QnLdapUsers>();

    /*
     * Following code requires full-scale refactor in the classes that uses signals with such parameters.
     * MOC-generated code contains names, based on signal methods declaration. Example:
     * ...
     * class QnStreamRecorder:
     * ...
     * signals:
     *     void recordingFinished(const ErrorStruct &status, const QString &fileName);
     * ...
     *
     * This code will require metatype, registered as:
     *    qRegisterMetaType<QnStreamRecorder::ErrorStruct>("ErrorStruct");
     *
     * Much more correct behavior is to change signal declaration to following:
     * ...
     * signals:
     *     void recordingFinished(const QnStreamRecorder::ErrorStruct &status, const QString &fileName);
     * ...
     * and therefore we can use simple registration:
     *    qRegisterMetaType<QnStreamRecorder::ErrorStruct>();
     */

    qRegisterMetaType<ec2::ErrorCode>( "ErrorCode" );
    qRegisterMetaType<ec2::AbstractECConnectionPtr>( "AbstractECConnectionPtr" );
    qRegisterMetaType<ec2::QnPeerTimeInfo>( "QnPeerTimeInfo" );
    qRegisterMetaType<ec2::QnPeerTimeInfoList>( "QnPeerTimeInfoList" );
    qRegisterMetaType<ec2::ApiPeerAliveData>( "ApiPeerAliveData" );
    qRegisterMetaType<ec2::ApiDiscoveryDataList>( "ApiDiscoveryDataList" );
    qRegisterMetaType<ec2::ApiDiscoveryData>( "ApiDiscoveryData" );
    qRegisterMetaType<ec2::ApiDiscoveredServerData>("ApiDiscoveredServerData");
    qRegisterMetaType<ec2::ApiDiscoveredServerDataList>("ApiDiscoveredServerDataList");
    qRegisterMetaType<ec2::ApiReverseConnectionData>( "ApiReverseConnectionData" );
    qRegisterMetaType<ec2::ApiRuntimeData>( "ApiRuntimeData" );
    qRegisterMetaType<ec2::ApiDatabaseDumpData>( "ApiDatabaseDumpData" );
    qRegisterMetaType<ec2::ApiDatabaseDumpToFileData>( "ApiDatabaseDumpToFileData" );
    qRegisterMetaType<ec2::ApiLockData>( "ApiLockData" );
    qRegisterMetaType<ec2::ApiResourceParamWithRefData>( "ApiResourceParamWithRefData" );
    qRegisterMetaType<ec2::ApiResourceParamWithRefDataList>("ApiResourceParamWithRefDataList");

    qRegisterMetaType<ec2::ApiResourceParamData>("ApiResourceParamData");
    qRegisterMetaType<ec2::ApiResourceParamDataList>("ApiResourceParamDataList");

    qRegisterMetaType<ec2::ApiServerFootageData>("ApiServerFootageData");
    qRegisterMetaType<ec2::ApiServerFootageDataList>("ApiServerFootageDataList");
    qRegisterMetaType<ec2::ApiCameraHistoryItemData>("ApiCameraHistoryItemData");
    qRegisterMetaType<ec2::ApiCameraHistoryItemDataList>("ApiCameraHistoryItemDataList");
    qRegisterMetaType<ec2::ApiCameraHistoryData>("ApiCameraHistoryData");
    qRegisterMetaType<ec2::ApiCameraHistoryDataList>("ApiCameraHistoryDataList");
    qRegisterMetaType<ec2::ApiCameraHistoryDataList>("ec2::ApiCameraHistoryDataList");

    qRegisterMetaType<ec2::ApiFullInfoData>("ec2::ApiFullInfoData");
    qRegisterMetaType<ec2::ApiUserData>("ec2::ApiUserData");
    qRegisterMetaType<ec2::ApiUserGroupData>("ec2::ApiUserGroupData");
    qRegisterMetaType<ec2::ApiAccessRightsData>("ec2::ApiAccessRightsData");
    qRegisterMetaType<ec2::ApiLayoutData>("ec2::ApiLayoutData");
    qRegisterMetaType<ec2::ApiLayoutItemData>("ec2::ApiLayoutItemData");
    qRegisterMetaType<ec2::ApiVideowallData>("ec2::ApiVideowallData");
    qRegisterMetaType<ec2::ApiVideowallControlMessageData>("ec2::ApiVideowallControlMessageData");
    qRegisterMetaType<ec2::ApiWebPageData>("ec2::ApiWebPageData");
    qRegisterMetaType<ec2::ApiCameraData>("ec2::ApiCameraData");
    qRegisterMetaType<ec2::ApiCameraAttributesData>("ec2::ApiCameraAttributesData");
    qRegisterMetaType<ec2::ApiMediaServerData>("ec2::ApiMediaServerData");
    qRegisterMetaType<ec2::ApiMediaServerUserAttributesData>("ec2::ApiMediaServerUserAttributesData");
    qRegisterMetaType<ec2::ApiStorageData>("ec2::ApiStorageData");
    qRegisterMetaType<ec2::ApiResourceParamWithRefDataList>("ec2::ApiResourceParamWithRefDataList");

    qRegisterMetaType<QnUuid>();
    qRegisterMetaTypeStreamOperators<QnUuid>();
    qRegisterMetaType<QnRecordingStatsReply>();
    qRegisterMetaType<QnAuditRecordList>();

    qRegisterMetaType<QnOptionalBool>();

    qRegisterMetaType<QnSystemHealth::MessageType>("QnSystemHealth::MessageType");

    qRegisterMetaType<QnServerFields>();

    QnJsonSerializer::registerSerializer<QnPtzMapperPtr>();
    QnJsonSerializer::registerSerializer<Qn::PtzTraits>();
    QnJsonSerializer::registerSerializer<Qn::PtzCapabilities>();

    QnJsonSerializer::registerSerializer<QnOnvifConfigDataPtr>();

    qn_commonMetaTypes_initialized = true;
}
