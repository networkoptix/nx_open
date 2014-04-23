#ifndef QN_CAMERA_DATA_I_H
#define QN_CAMERA_DATA_I_H

#include "api_data_i.h"
#include "ec2_resource_data_i.h"

namespace ec2 {

    struct ScheduleTaskData : ApiData
    {
        ScheduleTaskData(): startTime(0), endTime(0), doRecordAudio(false), recordType(Qn::RecordingType_Run), dayOfWeek(1), 
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

#define ScheduleTaskData_Fields (startTime)(endTime)(doRecordAudio)(recordType)(dayOfWeek)(beforeThreshold)(afterThreshold)(streamQuality)(fps) 


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
        std::vector<ScheduleTask>  scheduleTask;
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

#define ApiCameraData_Fields (scheduleDisabled)(motionType)(region)(mac)(login)(password)(scheduleTask)(audioEnabled)(physicalId)(manuallyAdded)(model) \
                            (firmware)(groupId)(groupName)(secondaryQuality)(controlDisabled)(statusFlags)(dewarpingParams)(vendor)
} // namespace ec2

#endif // QN_CAMERA_DATA_I_H
