#ifndef __API_CAMERA_DATA_H_
#define __API_CAMERA_DATA_H_

#include "resource_data.h"
#include "common/common_globals.h"
#include "core/resource/media_resource.h"
#include "core/resource/security_cam_resource.h"
#include "core/misc/schedule_task.h"

namespace ec2
{

struct ScheduleTask: public ApiData 
{
	ScheduleTask(): id(0), sourceId(0), startTime(0), endTime(0), doRecordAudio(false), recordType(Qn::RecordingType_Run), dayOfWeek(1), 
					beforeThreshold(0), afterThreshold(0), streamQuality(Qn::QualityNotDefined), fps(0.0) {}

	static ScheduleTask fromResource(const QnResourcePtr& cameraRes, const QnScheduleTask& resScheduleTask);
	QnScheduleTask toResource();

    qint32   id;
    qint32   sourceId;
    qint32   startTime;
    qint32   endTime;
    bool     doRecordAudio;
    Qn::RecordingType   recordType;
    qint32   dayOfWeek;
    qint32   beforeThreshold;
    qint32   afterThreshold;
    Qn::StreamQuality  streamQuality;
    qint32   fps;

    QN_DECLARE_STRUCT_SERIALIZATORS();
};

struct ApiCameraData: public ApiResourceData 
{
    bool                scheduleDisabled;
    Qn::MotionType      motionType;
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
    Qn::SecondStreamQuality    secondaryQuality;
    bool                controlDisabled;
    qint32				statusFlags;
    QByteArray          dewarpingParams;
    QString             vendor;

	void fromResource(const QnVirtualCameraResourcePtr& resource);
	void toResource(QnVirtualCameraResourcePtr resource);
    QN_DECLARE_STRUCT_SERIALIZATORS();
};

}

QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ScheduleTask, (id) (sourceId) (startTime) (endTime) (doRecordAudio) (recordType) (dayOfWeek) \
                                (beforeThreshold) (afterThreshold) (streamQuality) (fps) )

QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS (ec2::ApiCameraData, ApiResourceData, (scheduleDisabled) (motionType) (region) (mac) (login)\
                                        (password) (scheduleTask) (audioEnabled) (physicalId) (manuallyAdded) (model) (firmware) (groupId) (groupName) (secondaryQuality)\
                                        (controlDisabled) (statusFlags) (dewarpingParams) (vendor))

#endif // __API_CAMERA_DATA_H_
