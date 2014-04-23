#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "ec2_resource_data.h"
#include "ec2_bookmark_data.h"
#include "common/common_globals.h"
#include "core/resource/media_resource.h"
#include "core/resource/security_cam_resource.h"
#include "core/misc/schedule_task.h"
#include "core/resource/camera_resource.h"
#include <core/resource/camera_bookmark.h>
#include "utils/common/id.h"

namespace ec2
{
    #include "camera_data_i.h"

    void fromTaskToApi(const QnScheduleTask& scheduleTask, ScheduleTaskData &data);
    void fromApiToTask(const ScheduleTaskData &data, QnScheduleTask &scheduleTask);

    QN_DEFINE_STRUCT_SQL_BINDER(ScheduleTaskData, apiScheduleTaskFields);

    struct ScheduleTaskWithRef: ScheduleTaskData
    {
        QnId sourceId;
    };

    void fromResourceToApi(const QnVirtualCameraResourcePtr& resource, ApiCameraData &data);
    void fromApiToResource(const ApiCameraData &data, QnVirtualCameraResourcePtr& resource);

    QN_DEFINE_STRUCT_SQL_BINDER(ApiCameraData, apiCameraDataFields);

    struct ApiCameraList: ApiCameraDataListData
    {
        void loadFromQuery(QSqlQuery& query);

        template <class T> void toResourceList(QList<T>& outData, QnResourceFactory* factory) const;
        void fromResourceList(const QList<QnVirtualCameraResourcePtr>& cameras);
    };
}

#endif // __API_CAMERA_DATA_H_
