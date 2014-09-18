#include "common_meta_types.h"

#include <QtCore/QMetaType>

#include <utils/network/mac_address.h>
#include <utils/common/request_param.h>
#include <utils/serialization/json_functions.h>
#include <utils/network/multicast_module_finder.h>
#include <utils/math/space_mapper.h>

#include <api/model/storage_space_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/statistics_reply.h>
#include <api/model/camera_diagnostics_reply.h>
#include <api/model/manual_camera_seach_reply.h>
#include <api/model/servers_reply.h>
#include <api/model/kvpair.h>
#include <api/model/connection_info.h>
#include <api/model/time_reply.h>
#include <api/model/rebuild_archive_reply.h>
#include <api/model/test_email_settings_reply.h>
#include <api/model/configure_reply.h>
#include <api/runtime_info_manager.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_history.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>

#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource/videowall_matrix.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <core/misc/schedule_task.h>
#include <core/ptz/ptz_mapper.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/item_dewarping_params.h>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_event_rule.h>
#include <business/business_fwd.h>

#include <licensing/license.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_lock_data.h>
#include <nx_ec/data/api_discovery_data.h>

namespace {
    volatile bool qn_commonMetaTypes_initialized = false;
}

QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::Corner)

void QnCommonMetaTypes::initialize() {
    /* Note that running the code twice is perfectly OK, 
     * so we don't need heavyweight synchronization here. */
    if(qn_commonMetaTypes_initialized)
        return;

    qRegisterMetaType<QUuid>();
    qRegisterMetaType<QSet<QUuid>>("QSet<QUuid>");
    qRegisterMetaType<QHostAddress>();
    qRegisterMetaType<QAuthenticator>();
    qRegisterMetaType<Qt::ConnectionType>();
    qRegisterMetaType<Qt::Orientations>();

    qRegisterMetaType<QnMacAddress>();
    qRegisterMetaType<QnPeerRuntimeInfo>();

    qRegisterMetaType<QnParam>();
    
    qRegisterMetaType<QnKvPair>();
    qRegisterMetaType<QnKvPairList>();
    qRegisterMetaType<QnKvPairListsById>();

    qRegisterMetaType<QnResourceTypeList>();
    qRegisterMetaType<QnResourcePtr>();
    qRegisterMetaType<QnResourceList>();
    qRegisterMetaType<Qn::ResourceStatus>();
    qRegisterMetaType<QnBusiness::EventReason>();
    
    qRegisterMetaType<QnUserResourcePtr>();
    qRegisterMetaType<QnLayoutResourcePtr>();
    qRegisterMetaType<QnMediaServerResourcePtr>();
    qRegisterMetaType<QnVirtualCameraResourcePtr>();
    qRegisterMetaType<QnVirtualCameraResourceList>();
    qRegisterMetaType<QnSecurityCamResourcePtr>();
    qRegisterMetaType<QnAbstractStorageResourcePtr>();
    qRegisterMetaType<QnVideoWallResourcePtr>();

    qRegisterMetaType<QnUserResourceList>();
    qRegisterMetaType<QnVideoWallResourceList>();

    qRegisterMetaType<QnCameraHistoryList>();
    qRegisterMetaType<QnCameraHistoryItemPtr>();

    qRegisterMetaType<QnCameraBookmark>();
    qRegisterMetaType<QnCameraBookmarkList>();
    qRegisterMetaType<QnCameraBookmarkTags>("QnCameraBookmarkTags");/* The underlying type is identical to QStringList. */

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

    qRegisterMetaType<QnSoftwareVersion>();
    qRegisterMetaTypeStreamOperators<QnSoftwareVersion>();
    qRegisterMetaType<QnSystemInformation>();
    qRegisterMetaTypeStreamOperators<QnSystemInformation>();

    qRegisterMetaType<TypeSpecificParamMap>();
    qRegisterMetaType<QnStringBoolPairList>("QnStringBoolPairList");
    qRegisterMetaType<QnStringBoolPairList>("QList<QPair<QString,bool> >");
    qRegisterMetaType<QnStringVariantPairList>("QnStringVariantPairList");
    qRegisterMetaType<QnStringVariantPairList>("QList<QPair<QString,QVariant> >");
    qRegisterMetaType<QVector<int> >(); /* This one is used by QAbstractItemModel. */

#ifdef ENABLE_DATA_PROVIDERS
    qRegisterMetaType<QnMetaDataV1Ptr>();
#endif

    qRegisterMetaType<QnAbstractBusinessActionPtr>();
    qRegisterMetaType<QnAbstractBusinessActionList>();
    qRegisterMetaType<QnBusinessActionDataListPtr>();
    qRegisterMetaType<QnAbstractBusinessEventPtr>();
    qRegisterMetaType<QnBusinessEventRulePtr>();
    qRegisterMetaType<QnBusinessEventRuleList>();
    qRegisterMetaType<QnAbstractDataPacketPtr>();
    qRegisterMetaType<QnConstAbstractDataPacketPtr>();

    qRegisterMetaType<QnStorageSpaceReply>();
    qRegisterMetaType<QnStorageStatusReply>();
    qRegisterMetaType<QnStatisticsReply>();
    qRegisterMetaType<QnTimeReply>();
    qRegisterMetaType<QnTestEmailSettingsReply>();
    qRegisterMetaType<QnCameraDiagnosticsReply>();
    qRegisterMetaType<QnRebuildArchiveReply>();
    qRegisterMetaType<QnManualCameraSearchReply>();
    qRegisterMetaType<QnServersReply>();
    qRegisterMetaType<QnStatisticsData>();
    qRegisterMetaType<QnManualCameraSearchSingleCamera>();

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

    qRegisterMetaType<QnMediaDewarpingParams>();
    qRegisterMetaType<QnItemDewarpingParams>();

    qRegisterMetaType<Qn::Corner>();
    qRegisterMetaTypeStreamOperators<Qn::Corner>();

    qRegisterMetaType<QnConnectionInfo>();
    qRegisterMetaType<Qn::PanicMode>();

    qRegisterMetaType<QnModuleInformation>();

    qRegisterMetaType<Qn::CameraDataType>();

    qRegisterMetaType<QnNetworkAddress>();

    qRegisterMetaType<QnConfigureReply>();

    qRegisterMetaType<ec2::ErrorCode>( "ErrorCode" );
    qRegisterMetaType<ec2::AbstractECConnectionPtr>( "AbstractECConnectionPtr" );
    qRegisterMetaType<ec2::QnFullResourceData>( "QnFullResourceData" );
    qRegisterMetaType<ec2::QnPeerTimeInfo>( "QnPeerTimeInfo" );
    qRegisterMetaType<ec2::QnPeerTimeInfoList>( "QnPeerTimeInfoList" );
    qRegisterMetaType<ec2::ApiPeerAliveData>( "ApiPeerAliveData" ); 
    qRegisterMetaType<ec2::ApiDiscoveryDataList>( "ApiDiscoveryDataList" ); 
    qRegisterMetaType<ec2::ApiRuntimeData>( "ApiRuntimeData" ); 
    qRegisterMetaType<ec2::ApiDatabaseDumpData>( "ApiDatabaseDumpData" ); 
    qRegisterMetaType<ec2::ApiLockData>( "ApiLockData" ); 

    QnJsonSerializer::registerSerializer<QnPtzMapperPtr>();
    QnJsonSerializer::registerSerializer<Qn::PtzTraits>();
    QnJsonSerializer::registerSerializer<Qn::PtzCapabilities>();

    qn_commonMetaTypes_initialized = true;
}
