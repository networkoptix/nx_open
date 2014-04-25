#include "camera_data.h"
#include "core/resource/camera_resource.h"
#include "api/serializer/serializer.h"
#include "utils/common/json.h"
#include <QAuthenticator>
#include "core/resource/security_cam_resource.h"


namespace ec2 {

void fromTaskToApi(const QnScheduleTask& scheduleTask, ScheduleTaskData &data) {
	data.startTime = scheduleTask.getStartTime();
	data.endTime = scheduleTask.getEndTime();
	data.doRecordAudio = scheduleTask.getDoRecordAudio();
	data.recordType = scheduleTask.getRecordingType();
	data.dayOfWeek = scheduleTask.getDayOfWeek();
	data.beforeThreshold = scheduleTask.getBeforeThreshold();
	data.afterThreshold = scheduleTask.getAfterThreshold();
	data.streamQuality = scheduleTask.getStreamQuality();
	data.fps = scheduleTask.getFps();
}

void fromApiToTask(const ScheduleTaskData &data, QnScheduleTask &scheduleTask) {
    QnScheduleTask::Data taskData;
    taskData.m_startTime = data.startTime;
    taskData.m_endTime = data.endTime;
    taskData.m_doRecordAudio = data.doRecordAudio;
    taskData.m_recordType = data.recordType;
    taskData.m_dayOfWeek = data.dayOfWeek;
    taskData.m_beforeThreshold = data.beforeThreshold;
    taskData.m_afterThreshold = data.afterThreshold;
    taskData.m_streamQuality = data.streamQuality;
    taskData.m_fps = data.fps;
    
    scheduleTask.setData(taskData);
}

void fromResourceToApi(const QnVirtualCameraResourcePtr& resource, ApiCameraData &data) {
     fromResourceToApi(resource, (ApiResourceData &)data);

    data.scheduleDisabled = resource->isScheduleDisabled();
    data.motionType = resource->getMotionType();

    data.region = serializeMotionRegionList(resource->getMotionRegionList()).toLocal8Bit();
    data.mac = resource->getMAC().toString().toLocal8Bit();
    data.login = resource->getAuth().user();
    data.password = resource->getAuth().password();
    data.scheduleTask.clear();
    foreach(const QnScheduleTask &task, resource->getScheduleTasks()) {
        ScheduleTaskData taskData;
        fromTaskToApi(task, taskData);
        data.scheduleTask.push_back(taskData);
    }
    data.audioEnabled = resource->isAudioEnabled();
    data.physicalId = resource->getPhysicalId();
    data.manuallyAdded = resource->isManuallyAdded();
    data.model = resource->getModel();
    data.firmware = resource->getFirmware();
    data.groupId = resource->getGroupId();
    data.groupName = resource->getGroupName();
    data.secondaryQuality = resource->secondaryStreamQuality();
    data.controlDisabled = resource->isCameraControlDisabled();
    data.statusFlags = resource->statusFlags();
    data.dewarpingParams = QJson::serialized<QnMediaDewarpingParams>(resource->getDewarpingParams());
    data.vendor = resource->getVendor();
}

void fromApiToResource(const ApiCameraData &data, QnVirtualCameraResourcePtr& resource) {
    fromApiToResource((const ApiResourceData &)data, resource);

    resource->setScheduleDisabled(data.scheduleDisabled);
    resource->setMotionType(data.motionType);

    QList<QnMotionRegion> regions;
    parseMotionRegionList(regions, data.region);
    resource->setMotionRegionList(regions, QnDomainMemory);

    resource->setMAC(data.mac);
    QAuthenticator auth;
    auth.setUser(data.login);
    auth.setPassword(data.password);
    resource->setAuth(auth);


    QnScheduleTaskList tasks;
    tasks.reserve(data.scheduleTask.size());
    foreach(const ScheduleTaskData &taskData, data.scheduleTask) {
        QnScheduleTask task(data.id);
        fromApiToTask(taskData, task);
        tasks.push_back(task);
    }
    resource->setScheduleTasks(tasks);

    resource->setAudioEnabled(data.audioEnabled);
    resource->setPhysicalId(data.physicalId);
    resource->setManuallyAdded(data.manuallyAdded);
    resource->setModel(data.model);
    resource->setFirmware(data.firmware);
    resource->setGroupId(data.groupId);
    resource->setGroupName(data.groupName);
    resource->setSecondaryStreamQuality(data.secondaryQuality);
    resource->setCameraControlDisabled(data.controlDisabled);
    resource->setStatusFlags((QnSecurityCamResource::StatusFlags) data.statusFlags);

    resource->setDewarpingParams(QJson::deserialized<QnMediaDewarpingParams>(data.dewarpingParams));
    resource->setVendor(data.vendor);
}

template <class T> 
void ApiCameraList::toResourceList(QList<T>& outData, QnResourceFactory* factory) const
{
    outData.reserve(outData.size() + data.size());
    for(int i = 0; i < data.size(); ++i) 
    {
        QnVirtualCameraResourcePtr camera = factory->createResource(data[i].typeId, QnResourceParams(data[i].url, data[i].vendor)).dynamicCast<QnVirtualCameraResource>();
        if (camera) {
            fromApiToResource(data[i], camera);
            outData << camera;
        }
    }
}

void ApiCameraList::fromResourceList(const QList<QnVirtualCameraResourcePtr>& cameras)
{
    data.resize(cameras.size());
    for(int i = 0; i < cameras.size(); ++i)
        fromResourceToApi(cameras[i], data[i]);
}


template void ApiCameraList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData, QnResourceFactory* factory) const;
template void ApiCameraList::toResourceList<QnVirtualCameraResourcePtr>(QList<QnVirtualCameraResourcePtr>& outData, QnResourceFactory* factory) const;


void ApiCameraList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiCameraData, data, apiCameraDataFields ApiResourceFields)
}

}
