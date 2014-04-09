#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "ec2_resource_data.h"
#include "common/common_globals.h"
#include "core/resource/media_resource.h"
#include "core/resource/security_cam_resource.h"
#include "core/misc/schedule_task.h"
#include "core/resource/camera_resource.h"
#include "utils/common/id.h"

namespace ec2
{
    struct ScheduleTask;
    struct ApiCamera;
    #include "camera_data_i.h"

    struct ScheduleTask: ScheduleTaskData
    {
        static ScheduleTask fromResource(const QnResourcePtr& cameraRes, const QnScheduleTask& resScheduleTask);
        QnScheduleTask toResource(const QnId& resourceId) const;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ScheduleTask, apiScheduleTaskFields);

    struct ScheduleTaskWithRef: ScheduleTask
    {
        QnId sourceId;
    };

    struct ApiCamera: ApiCameraData, ApiResource
    {
        void fromResource(const QnVirtualCameraResourcePtr& resource);
        void toResource(QnVirtualCameraResourcePtr resource) const;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiCamera, apiCameraDataFields);

    struct ApiCameraList: ApiCameraListData
    {
        void loadFromQuery(QSqlQuery& query);

        template <class T> void toResourceList(QList<T>& outData, QnResourceFactory* factory) const;
        void fromResourceList(const QList<QnVirtualCameraResourcePtr>& cameras);
    };
}

#endif // __API_CAMERA_DATA_H_
