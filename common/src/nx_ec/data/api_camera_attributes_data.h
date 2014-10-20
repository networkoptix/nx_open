/**********************************************************
* 2 oct 2014
* akolesnikov
***********************************************************/

#ifndef __API_CAMERA_ATTRIBUTES_DATA_H_
#define __API_CAMERA_ATTRIBUTES_DATA_H_

#include "api_globals.h"
#include "api_data.h"


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
        QnUuid sourceId;
    };
#define ApiScheduleTaskWithRefData_Fields ApiScheduleTaskData_Fields (sourceId)


    struct ApiCameraAttributesData : ApiData
    {
        ApiCameraAttributesData(): scheduleEnabled(true), motionType(Qn::MT_Default), audioEnabled(false), secondaryStreamQuality(Qn::SSQualityNotDefined),
                         controlEnabled(true), minArchiveDays(0), maxArchiveDays(0) {}

        QnUuid              cameraID;
        QString             cameraName;
        bool                scheduleEnabled;
        Qn::MotionType      motionType;
        QnLatin1Array       motionMask;
        std::vector<ApiScheduleTaskData>  scheduleTasks;
        bool                audioEnabled;
        Qn::SecondStreamQuality  secondaryStreamQuality;
        bool                controlEnabled;
        QnLatin1Array       dewarpingParams;
        int                 minArchiveDays;
        int                 maxArchiveDays;
        QnUuid              preferedServerId;
    };
#define ApiCameraAttributesData_Fields_Short (scheduleEnabled)(motionType)(motionMask)(scheduleTasks)(audioEnabled)(secondaryStreamQuality) \
    (controlEnabled)(dewarpingParams)(minArchiveDays)(maxArchiveDays)(preferedServerId)
#define ApiCameraAttributesData_Fields (cameraID) (cameraName) ApiCameraAttributesData_Fields_Short

} // namespace ec2

#endif // __API_CAMERA_ATTRIBUTES_DATA_H_
