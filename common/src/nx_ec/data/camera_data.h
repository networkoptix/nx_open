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

struct ScheduleTask: public ApiData 
{
	ScheduleTask(): startTime(0), endTime(0), doRecordAudio(false), recordType(Qn::RecordingType_Run), dayOfWeek(1), 
					beforeThreshold(0), afterThreshold(0), streamQuality(Qn::QualityNotDefined), fps(0.0) {}

	static ScheduleTask fromResource(const QnResourcePtr& cameraRes, const QnScheduleTask& resScheduleTask);
	QnScheduleTask toResource(const QnId& resourceId) const;

    //qint32   id;
    //qint32   sourceId;
    qint32   startTime;
    qint32   endTime;
    bool     doRecordAudio;
    Qn::RecordingType   recordType;
    qint32   dayOfWeek;
    qint32   beforeThreshold;
    qint32   afterThreshold;
    Qn::StreamQuality  streamQuality;
    qint32   fps;

    QN_DECLARE_STRUCT_SQL_BINDER();
};

struct ScheduleTaskWithRef: public ScheduleTask
{
    QnId sourceId;
};


struct ApiCameraData: public ApiResourceData 
{
    ApiCameraData(): scheduleDisabled(false), motionType(Qn::MT_Default), audioEnabled(false), manuallyAdded(false), secondaryQuality(Qn::SSQualityNotDefined),
                     controlDisabled(false), statusFlags(0) {}

    bool                scheduleDisabled;
    Qn::MotionType      motionType;
    QByteArray          region;
    QByteArray          mac;
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
	void toResource(QnVirtualCameraResourcePtr resource) const;
    QN_DECLARE_STRUCT_SQL_BINDER();
};

struct ApiCameraDataList: public ApiData
{
	std::vector<ApiCameraData> data;
	
	void loadFromQuery(QSqlQuery& query);

	template <class T> void toResourceList(QList<T>& outData, QnResourceFactory* factory) const;
    void fromResourceList(const QList<QnVirtualCameraResourcePtr>& cameras);
};

}

#define apiScheduleTaskFields (startTime) (endTime) (doRecordAudio) (recordType) (dayOfWeek) (beforeThreshold) (afterThreshold) (streamQuality) (fps) 
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ScheduleTask, apiScheduleTaskFields)
#define apiCameraDataFields (scheduleDisabled) (motionType) (region) (mac) (login) (password) (scheduleTask) (audioEnabled) (physicalId) (manuallyAdded) (model) \
							(firmware) (groupId) (groupName) (secondaryQuality) (controlDisabled) (statusFlags) (dewarpingParams) (vendor)
QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiCameraData, ec2::ApiResourceData, apiCameraDataFields)
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiCameraDataList, (data) )

#endif // __API_CAMERA_DATA_H_
