#include "common_meta_types.h"

#include <QtCore/QMetaType>

#include <utils/network/mac_address.h>
#include <utils/common/request_param.h>

#include <api/message.h>
#include <api/media_server_cameras_data.h>
#include <api/media_server_statistics_data.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource/layout_item_data.h>
#include <core/misc/schedule_task.h>


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

    qRegisterMetaType<QnMacAddress>();

    qRegisterMetaType<QnParam>();
    qRegisterMetaType<QnId>();

    qRegisterMetaType<QnResourcePtr>();
    qRegisterMetaType<QnResourceList>();
    qRegisterMetaType<QnResource::Status>();
    qRegisterMetaType<QnVirtualCameraResourcePtr>();
    qRegisterMetaType<QnUserResourcePtr>();

    qRegisterMetaType<QnLayoutItemData>();
    qRegisterMetaType<QnMotionRegion>();
    qRegisterMetaType<QnScheduleTask>();
    qRegisterMetaType<QnScheduleTaskList>();

    qRegisterMetaType<QnRequestParamList>();
    qRegisterMetaType<QnMessage>();

    qRegisterMetaType<QnCamerasFoundInfoList>();
    qRegisterMetaType<QnStatisticsDataList>();

    qRegisterMetaType<Qn::TimePeriodRole>();
    qRegisterMetaType<QnTimePeriodList>();

    qn_commonMetaTypes_initialized = true;
}