#include "common_meta_types.h"

#include <QtCore/QMetaType>

#include <utils/network/mac_address.h>
#include <utils/common/request_param.h>
#include <utils/common/json_serializer.h>
#include <utils/appcast/update_info.h>
#include <utils/network/networkoptixmodulerevealcommon.h>
#include <utils/math/space_mapper.h>

#include <api/model/storage_space_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/statistics_reply.h>
#include <api/model/camera_diagnostics_reply.h>
#include <api/model/manual_camera_seach_reply.h>
#include <api/model/servers_reply.h>
#include <api/model/kvpair.h>
#include <api/model/connection_info.h>
#include <api/message.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_history.h>
#include <core/misc/schedule_task.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/item_dewarping_params.h>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_fwd.h>

#include <licensing/license.h>


namespace {
    volatile bool qn_commonMetaTypes_initialized = false;
}

QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::Corner)

void QnCommonMetaTypes::initilize() {
    /* Note that running the code twice is perfectly OK, 
     * so we don't need heavyweight synchronization here. */
    if(qn_commonMetaTypes_initialized)
        return;

    qRegisterMetaType<QnConnectionInfoPtr>();

    qRegisterMetaType<QUuid>();
    qRegisterMetaType<QHostAddress>();
    qRegisterMetaType<QAuthenticator>();
    qRegisterMetaType<Qt::ConnectionType>();
    qRegisterMetaType<Qt::Orientations>();

    qRegisterMetaType<QnMacAddress>();

    qRegisterMetaType<QnParam>();
    qRegisterMetaType<QnId>();
    
    qRegisterMetaType<QnKvPair>();
    qRegisterMetaType<QnKvPairList>();
    qRegisterMetaType<QnKvPairListsById>();

    qRegisterMetaType<QnResourceTypeList>();
    qRegisterMetaType<QnResourcePtr>();
    qRegisterMetaType<QnResourceList>();
    qRegisterMetaType<QnResource::Status>();
    qRegisterMetaType<QnBusiness::EventReason>();
    
    qRegisterMetaType<QnUserResourcePtr>();
    qRegisterMetaType<QnLayoutResourcePtr>();
    qRegisterMetaType<QnMediaServerResourcePtr>();
    qRegisterMetaType<QnVirtualCameraResourcePtr>();
    qRegisterMetaType<QnSecurityCamResourcePtr>();
    qRegisterMetaType<QnAbstractStorageResourcePtr>();

    qRegisterMetaType<QnCameraHistoryList>();

    qRegisterMetaType<QnLicenseList>();

    qRegisterMetaType<QnLayoutItemData>();
    qRegisterMetaType<QnMotionRegion>();
    qRegisterMetaType<QnScheduleTask>();
    qRegisterMetaType<QnScheduleTaskList>();

    qRegisterMetaType<QnRequestParamList>();
    qRegisterMetaType<QnRequestHeaderList>("QnRequestHeaderList"); /* The underlying type is identical to QnRequestParamList. */
    qRegisterMetaType<QnReplyHeaderList>();
    qRegisterMetaType<QnHTTPRawResponse>();

    qRegisterMetaType<QnMessage>();

    qRegisterMetaType<Qn::TimePeriodContent>();
    qRegisterMetaType<QnTimePeriodList>();

    qRegisterMetaType<QnSoftwareVersion>();
    qRegisterMetaTypeStreamOperators<QnSoftwareVersion>();
    qRegisterMetaType<QnUpdateInfoItem>();
    qRegisterMetaType<QnUpdateInfoItemList>();

    qRegisterMetaType<TypeSpecificParamMap>();
    qRegisterMetaType<QnStringBoolPairList>("QnStringBoolPairList");
    qRegisterMetaType<QnStringBoolPairList>("QList<QPair<QString,bool> >");
    qRegisterMetaType<QnStringVariantPairList>("QnStringVariantPairList");
    qRegisterMetaType<QnStringVariantPairList>("QList<QPair<QString,QVariant> >");
    qRegisterMetaType<QVector<int> >(); /* This one is used by QAbstractItemModel. */

    qRegisterMetaType<QnAbstractBusinessActionPtr>();
    qRegisterMetaType<QnAbstractBusinessActionList>();
    qRegisterMetaType<QnBusinessActionDataListPtr>();
    qRegisterMetaType<QnAbstractBusinessEventPtr>();
    qRegisterMetaType<QnMetaDataV1Ptr>();
    qRegisterMetaType<QnBusinessEventRulePtr>();
    qRegisterMetaType<QnBusinessEventRuleList>();
    qRegisterMetaType<QnAbstractDataPacketPtr>();
    qRegisterMetaType<QnConstAbstractDataPacketPtr>();

    qRegisterMetaType<QnStorageSpaceReply>();
    qRegisterMetaType<QnStorageStatusReply>();
    qRegisterMetaType<QnStatisticsReply>();
    qRegisterMetaType<QnTimeReply>();
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
    qRegisterMetaType<Qn::PtzDataFields>();
    qRegisterMetaType<Qn::PtzCommand>();

    qRegisterMetaType<QnMediaDewarpingParams>();
    qRegisterMetaType<QnItemDewarpingParams>();

    qRegisterMetaType<Qn::Corner>();
    qRegisterMetaTypeStreamOperators<Qn::Corner>();

    qRegisterMetaType<QnConnectionInfo>();
    qRegisterMetaType<ec2::QnFullResourceData>();

    QnJsonSerializer::registerSerializer<QUuid>();

    qn_commonMetaTypes_initialized = true;
}
