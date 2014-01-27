#include "camera_data.h"
#include "core/resource/camera_resource.h"
#include "api/serializer/serializer.h"
#include "utils/common/json.h"

namespace ec2 {

ScheduleTask ScheduleTask::fromResource(const QnResourcePtr& cameraRes, const QnScheduleTask& resScheduleTask)
{
	ScheduleTask result;
	result.sourceId = cameraRes->getId();
	result.startTime = resScheduleTask.getStartTime();
	result.endTime = resScheduleTask.getEndTime();
	result.doRecordAudio = resScheduleTask.getDoRecordAudio();
	result.recordType = resScheduleTask.getRecordingType();
	result.dayOfWeek = resScheduleTask.getDayOfWeek();
	result.beforeThreshold = resScheduleTask.getBeforeThreshold();
	result.afterThreshold = resScheduleTask.getAfterThreshold();
	result.streamQuality = resScheduleTask.getStreamQuality();
	result.fps = resScheduleTask.getFps();

	return result;
}


void ApiCameraData::fromResource(const QnVirtualCameraResourcePtr& resource)
{
	ApiResourceData::fromResource(resource);

	scheduleDisabled = resource->isScheduleDisabled();
	motionType = resource->getMotionType();

	QList<QnMotionRegion> regions;
	region = serializeMotionRegionList(resource->getMotionRegionList());
	mac = resource->getMAC().toString();
	login = resource->getAuth().user();
	password = resource->getAuth().password();
	scheduleTask.clear();
	foreach(QnScheduleTask task, resource->getScheduleTasks())
		scheduleTask.push_back(ScheduleTask::fromResource(resource, task));
	audioEnabled = resource->isAudioEnabled();
	physicalId = resource->getPhysicalId();
	manuallyAdded = resource->isManuallyAdded();
	model = resource->getModel();
	firmware = resource->getFirmware();
	groupId = resource->getGroupId();
	groupName = resource->getGroupName();
	secondaryQuality = resource->getSecondaryStreamQuality();
	controlDisabled = resource->isCameraControlDisabled();
	statusFlags = resource->statusFlags();
	dewarpingParams = QJson::serialized<QnMediaDewarpingParams>(resource->getDewarpingParams());
	vendor = resource->getVendor();
}

}
