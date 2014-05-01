#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "api_resource_data.h"

namespace ec2
{
    struct ApiScheduleTaskData : ApiData
    {
        ApiScheduleTaskData(): startTime(0), endTime(0), recordAudio(false), recordingType(Qn::RecordingType_Run), dayOfWeek(1), 
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
        QnId sourceId;
    };
#define ApiScheduleTaskWithRefData_Fields ApiScheduleTaskData_Fields (sourceId)


    struct ApiCameraData: ApiResourceData
    {
        ApiCameraData(): scheduleDisabled(false), motionType(Qn::MT_Default), audioEnabled(false), manuallyAdded(false), secondaryStreamQuality(Qn::SSQualityNotDefined),
                         controlDisabled(false), statusFlags(0) {}

        bool                scheduleDisabled; // TODO: #Elric #EC2 ENABLED!!!!
        Qn::MotionType      motionType;
        QByteArray          region; // TODO: #Elric #EC2 rename. what region?
        QnLatin1Array       mac;
        QString             login;
        QString             password;
        std::vector<ApiScheduleTaskData>  scheduleTasks;
        bool                audioEnabled;
        QString             physicalId;
        bool                manuallyAdded;
        QString             model;
        QString             firmware;
        QString             groupId;
        QString             groupName;
        Qn::SecondStreamQuality    secondaryStreamQuality;
        bool                controlDisabled; // TODO: #Elric ENABLED!!!
        qint32              statusFlags; // TODO: #Elric #EC2 QnSecurityCamResource::StatusFlags
        QByteArray          dewarpingParams;
        QString             vendor;
    };
#define ApiCameraData_Fields ApiResourceData_Fields (scheduleDisabled)(motionType)(region)(mac)(login)(password)(scheduleTasks)(audioEnabled)(physicalId)(manuallyAdded)(model) \
                            (firmware)(groupId)(groupName)(secondaryStreamQuality)(controlDisabled)(statusFlags)(dewarpingParams)(vendor)

} // namespace ec2

#endif // __API_CAMERA_DATA_H_
