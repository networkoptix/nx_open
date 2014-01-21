#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "resource_data.h"

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

struct ScheduleTask {
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

    template <class T> void serialize(BinaryStream<T>& stream);
};

namespace detail {
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS (ec2::ApiCameraData, (scheduleDisabled) (motionType) (region) (mac) (login)\
        (password) (scheduleTask) (audioEnabled) (physicalId) (manuallyAdded) (model) (firmware) (groupId) (groupName) (secondaryQuality)\
        (controlDisabled) (statusFlags) (dewarpingParams) (vendor) )
}

template <class T>
void ApiCameraData::serialize(BinaryStream<T>& stream)
{
    ApiResourceData::serialize(stream);
    detail::serialize(*this, &stream);
}

}

#endif // __API_CAMERA_DATA_H_
