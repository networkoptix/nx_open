#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "resource_data.h"
#include "serialization_helper.h"


namespace ec2
{

enum MotionType {
    Default      = 0,
    HardwareGrid = 1,
    SoftwareGrid = 2,
    MotionWindow = 4,
    NoMotion     = 8
};

enum Quality {
    Lowest  = 0,
    Low     = 1,
    Normal  = 2,
    High    = 3,
    Highest = 4,
    Preset  = 5
};

enum SecondaryQuality {
    SecondaryLow        = 0,
    SecondaryMedium     = 1,
    SecondaryHigh       = 2,
    SecondaryNotDefined = 3,
    SecondaryDontUse    = 4
};

struct ScheduleTask: public ApiData {
    qint32   id;
    qint32   sourceId;
    qint32   startTime;
    qint32   endTime;
    bool     doRecordAudio;
    qint32   recordType;
    qint32   dayOfWeek;
    qint32   beforeThreshold;
    qint32   afterThreshold;
    Quality  streamQuality;
    qint32   fps;

    QN_DECLARE_STRUCT_SERIALIZATORS();
};

struct ApiCameraData: public ApiResourceData 
{
    bool                scheduleDisabled;
    MotionType          motionType;
    QString             region;
    QString             mac;
    QString             login;
    QString             password;
    std::vector<ScheduleTask> scheduleTask;
    bool                audioEnabled;
    QString             physicalId;
    bool                manuallyAdded;
    QString             model;
    QString             firmware;
    QString             groupId;
    QString             groupName;
    SecondaryQuality    secondaryQuality;
    bool                controlDisabled;
    qint32              statusFlags;
    QString             dewarpingParams;
    QString             vendor;

    QN_DECLARE_STRUCT_SERIALIZATORS();
};

}

QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ScheduleTask, (id) (sourceId) (startTime) (endTime) (doRecordAudio) (recordType) (dayOfWeek) \
                                (beforeThreshold) (afterThreshold) (streamQuality) (fps) )

QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS (ec2::ApiCameraData, ApiResourceData, (scheduleDisabled) (motionType) (region) (mac) (login)\
                                        (password) (scheduleTask) (audioEnabled) (physicalId) (manuallyAdded) (model) (firmware) (groupId) (groupName) (secondaryQuality)\
                                        (controlDisabled) (statusFlags) (dewarpingParams) (vendor))

#endif // __API_CAMERA_DATA_H_
