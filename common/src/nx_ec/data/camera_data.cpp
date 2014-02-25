#include "camera_data.h"
#include "core/resource/camera_resource.h"
#include "api/serializer/serializer.h"
#include "utils/common/json.h"
#include <QAuthenticator>
#include "core/resource/security_cam_resource.h"


namespace ec2 {

ScheduleTask ScheduleTask::fromResource(const QnResourcePtr& cameraRes, const QnScheduleTask& resScheduleTask)
{
	ScheduleTask result;
	//result.sourceId = cameraRes->getId();
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

QnScheduleTask ScheduleTask::toResource(const QnId& resourceId) const
{
	return QnScheduleTask(resourceId, dayOfWeek, startTime, endTime, recordType, beforeThreshold, afterThreshold, streamQuality, fps, doRecordAudio);
}

void ApiCameraData::toResource(QnVirtualCameraResourcePtr resource) const
{
	ApiResourceData::toResource(resource);

	resource->setScheduleDisabled(scheduleDisabled);
	resource->setMotionType(motionType);

	QList<QnMotionRegion> regions;
	parseMotionRegionList(regions, region);
	resource->setMotionRegionList(regions, QnDomainMemory);

	resource->setMAC(mac);
	QAuthenticator auth;
	auth.setUser(login);
	auth.setPassword(password);
	resource->setAuth(auth);


	QnScheduleTaskList tasks;
    tasks.reserve(scheduleTask.size());
	foreach(ScheduleTask task, scheduleTask)
		tasks.push_back(task.toResource(id));
	resource->setScheduleTasks(tasks);

	resource->setAudioEnabled(audioEnabled);
	resource->setPhysicalId(physicalId);
	resource->setManuallyAdded(manuallyAdded);
	resource->setModel(model);
	resource->setFirmware(firmware);
	resource->setGroupId(groupId);
	resource->setGroupName(groupName);
	resource->setSecondaryStreamQuality(secondaryQuality);
	resource->setCameraControlDisabled(controlDisabled);
	resource->setStatusFlags((QnSecurityCamResource::StatusFlags) statusFlags);

	resource->setDewarpingParams(QJson::deserialized<QnMediaDewarpingParams>(dewarpingParams));
	resource->setVendor(vendor);
}

void ApiCameraData::fromResource(const QnVirtualCameraResourcePtr& resource)
{
    ApiResourceData::fromResource(resource);

	scheduleDisabled = resource->isScheduleDisabled();
	motionType = resource->getMotionType();


	QList<QnMotionRegion> regions;
	region = serializeMotionRegionList(resource->getMotionRegionList()).toLocal8Bit();
	mac = resource->getMAC().toString().toLocal8Bit();
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
	secondaryQuality = resource->secondaryStreamQuality();
	controlDisabled = resource->isCameraControlDisabled();
	statusFlags = resource->statusFlags();
	dewarpingParams = QJson::serialized<QnMediaDewarpingParams>(resource->getDewarpingParams());
	vendor = resource->getVendor();
}

template <class T> 
void ApiCameraDataList::toResourceList(QList<T>& outData, QnResourceFactory* factory) const
{
    outData.reserve(outData.size() + data.size());
    for(int i = 0; i < data.size(); ++i) 
    {
        QnVirtualCameraResourcePtr camera = factory->createResource(data[i].typeId, data[i].toHashMap()).dynamicCast<QnVirtualCameraResource>();
        if (camera) {
            data[i].toResource(camera);
            outData << camera;
        }
    }
}

void ApiCameraDataList::fromResourceList(const QList<QnVirtualCameraResourcePtr>& cameras)
{
    data.resize(cameras.size());
    for(int i = 0; i < cameras.size(); ++i)
        data[i].fromResource(cameras[i]);
}


template void ApiCameraDataList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData, QnResourceFactory* factory) const;
template void ApiCameraDataList::toResourceList<QnVirtualCameraResourcePtr>(QList<QnVirtualCameraResourcePtr>& outData, QnResourceFactory* factory) const;


void ApiCameraDataList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiCameraData, data, apiCameraDataFields ApiResourceDataFields)
}

}
