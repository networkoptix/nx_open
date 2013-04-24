#include "common_meta_types.h"

#include <QtCore/QMetaType>

#include <utils/network/mac_address.h>
#include <utils/common/request_param.h>
#include <utils/appcast/update_info.h>
#include <utils/network/networkoptixmodulerevealcommon.h>
#include <utils/math/space_mapper.h>

#include <api/model/storage_space_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/message.h>
#include <api/media_server_cameras_data.h>
#include <api/media_server_statistics_data.h>
#include <api/media_server_connection.h>

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

#include <core/misc/schedule_task.h>
#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include "business/business_logic_common.h"

namespace {
    volatile bool qn_commonMetaTypes_initialized = false;
}

void QnCommonMetaTypes::initilize() {
    /* Note that running the code twice is perfectly OK, 
     * so we don't need heavyweight synchronization here. */
    if(qn_commonMetaTypes_initialized)
        return;

    qRegisterMetaType<QUuid>();
    qRegisterMetaType<QHostAddress>();
    qRegisterMetaType<QAuthenticator>();
    qRegisterMetaType<Qt::ConnectionType>();

    qRegisterMetaType<QnMacAddress>();

    qRegisterMetaType<QnParam>();
    qRegisterMetaType<QnId>();

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


    qRegisterMetaType<QnLayoutItemData>();
    qRegisterMetaType<QnMotionRegion>();
    qRegisterMetaType<QnScheduleTask>();
    qRegisterMetaType<QnScheduleTaskList>();

    qRegisterMetaType<QnRequestParamList>();
    qRegisterMetaType<QnRequestHeaderList>("QnRequestHeaderList"); /* The underlying type is identical to QnRequestParamList. */
    qRegisterMetaType<QnReplyHeaderList>();
    qRegisterMetaType<QnHTTPRawResponse>();

    qRegisterMetaType<QnMessage>();

    qRegisterMetaType<QnCamerasFoundInfoList>();
    qRegisterMetaType<QnStatisticsDataList>();
    qRegisterMetaType<QnStatisticsData>();

    qRegisterMetaType<QnPtzSpaceMapper>();

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

    qRegisterMetaType<QnAbstractBusinessActionPtr>();
    qRegisterMetaType<QnAbstractBusinessEventPtr>();
    qRegisterMetaType<QnMetaDataV1Ptr>();
    qRegisterMetaType<QnBusinessEventRulePtr>();
    qRegisterMetaType<QnAbstractDataPacketPtr>();

    qRegisterMetaType<QnStorageSpaceReply>();
    qRegisterMetaType<QnStorageStatusReply>();
    
    qn_commonMetaTypes_initialized = true;
}
