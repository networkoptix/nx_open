#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiScheduleTaskData : ApiData
    {
        ApiScheduleTaskData(): startTime(0), endTime(0), doRecordAudio(false), recordType(Qn::RecordingType_Run), dayOfWeek(1), 
                        beforeThreshold(0), afterThreshold(0), streamQuality(Qn::QualityNotDefined), fps(0.0) {}

        qint32   startTime;
        qint32   endTime;
        bool     doRecordAudio;
        Qn::RecordingType   recordType;
        qint8    dayOfWeek;
        qint16   beforeThreshold;
        qint16   afterThreshold;
        Qn::StreamQuality  streamQuality;
        qint16   fps;
    };
#define ApiScheduleTaskData_Fields (startTime)(endTime)(doRecordAudio)(recordType)(dayOfWeek)(beforeThreshold)(afterThreshold)(streamQuality)(fps) 


    struct ApiScheduleTaskWithRefData: ApiScheduleTaskData
    {
        QnId sourceId;
    };
#define ApiScheduleTaskWithRefData_Fields ApiScheduleTaskData_Fields (sourceId)


    struct ApiCameraData: ApiResourceData
    {
        ApiCameraData(): scheduleDisabled(false), motionType(Qn::MT_Default), audioEnabled(false), manuallyAdded(false), secondaryQuality(Qn::SSQualityNotDefined),
                         controlDisabled(false), statusFlags(0) {}

        bool                scheduleDisabled;
        Qn::MotionType      motionType;
        QByteArray          region;
        QByteArray          mac;
        QString             login;
        QString             password;
        std::vector<ApiScheduleTaskData>  scheduleTask;
        bool                audioEnabled;
        QString             physicalId;
        bool                manuallyAdded;
        QString             model;
        QString             firmware;
        QString             groupId;
        QString             groupName;
        Qn::SecondStreamQuality    secondaryQuality;
        bool                controlDisabled;
        qint32              statusFlags;
        QByteArray          dewarpingParams;
        QString             vendor;
    };
#define ApiCameraData_Fields ApiResourceData_Fields (scheduleDisabled)(motionType)(region)(mac)(login)(password)(scheduleTask)(audioEnabled)(physicalId)(manuallyAdded)(model) \
                            (firmware)(groupId)(groupName)(secondaryQuality)(controlDisabled)(statusFlags)(dewarpingParams)(vendor)

    /*struct ScheduleTask: ApiScheduleTaskData
    {
        static ScheduleTask fromResource(const QnResourcePtr& cameraRes, const QnScheduleTask& resScheduleTask);
        QnScheduleTask toResource(const QnId& resourceId) const;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    //QN_DEFINE_STRUCT_SQL_BINDER(ScheduleTask, apiScheduleTaskFields);

    struct ApiCamera: ApiCameraData
    {
        void fromResource(const QnVirtualCameraResourcePtr& resource);
        void toResource(QnVirtualCameraResourcePtr resource) const;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    //QN_DEFINE_STRUCT_SQL_BINDER(ApiCamera, apiCameraDataFields);

    struct ApiCameraList: ApiCameraListData
    {
        void loadFromQuery(QSqlQuery& query);

        template <class T> void toResourceList(QList<T>& outData, QnResourceFactory* factory) const;
        void fromResourceList(const QList<QnVirtualCameraResourcePtr>& cameras);
    };*/
} // namespace ec2

//QN_FUSION_DECLARE_FUNCTIONS(ApiScheduleTaskData, (binary))


#endif // __API_CAMERA_DATA_H_
