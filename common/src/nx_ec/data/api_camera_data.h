#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiScheduleTaskData : ApiData
    {
        ApiScheduleTaskData(): startTime(0), endTime(0), recordAudio(false), recordingType(Qn::RT_Always), dayOfWeek(1), 
                        beforeThreshold(0), afterThreshold(0), streamQuality(Qn::QualityNotDefined), fps(0.0) {}

        qint32              startTime;
        qint32              endTime;
        bool                recordAudio;
        Qn::RecordingType   recordingType;
        qint8               dayOfWeek;
        qint16              beforeThreshold;
        qint16              afterThreshold;
        Qn::StreamQuality   streamQuality;
        qint16              fps;
    };
#define ApiScheduleTaskData_Fields (startTime)(endTime)(recordAudio)(recordingType)(dayOfWeek)(beforeThreshold)(afterThreshold)(streamQuality)(fps) 


    struct ApiScheduleTaskWithRefData: ApiScheduleTaskData
    {
        QUuid sourceId;
    };
#define ApiScheduleTaskWithRefData_Fields ApiScheduleTaskData_Fields (sourceId)


    struct ApiCameraData: ApiResourceData
    {
        ApiCameraData(): scheduleEnabled(true), motionType(Qn::MT_Default), audioEnabled(false), manuallyAdded(false), secondaryStreamQuality(Qn::SSQualityNotDefined),
                         controlEnabled(true), statusFlags(0), minArchiveDays(0), maxArchiveDays(0) {}

        bool                scheduleEnabled;
        Qn::MotionType      motionType;
        QnLatin1Array       motionMask;
        QnLatin1Array       mac;
        QString             login;
        QString             password;
        std::vector<ApiScheduleTaskData>  scheduleTasks;
        bool                audioEnabled;
        QString             physicalId;
        bool                manuallyAdded;
        QString             model;
        QString             groupId;
        QString             groupName;
        Qn::SecondStreamQuality  secondaryStreamQuality;
        bool                controlEnabled;
        Qn::CameraStatusFlags   statusFlags;
        QnLatin1Array       dewarpingParams;
        QString             vendor;
        int                 minArchiveDays;
        int                 maxArchiveDays;
        QUuid               preferedServerId;
    };
#define ApiCameraData_Fields ApiResourceData_Fields (scheduleEnabled)(motionType)(motionMask)(mac)(login)(password)(scheduleTasks)(audioEnabled)(physicalId)(manuallyAdded)(model) \
                            (groupId)(groupName)(secondaryStreamQuality)(controlEnabled)(statusFlags)(dewarpingParams)(vendor)(minArchiveDays)(maxArchiveDays)(preferedServerId)

} // namespace ec2

#endif // __API_CAMERA_DATA_H_
