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

void ApiCameraBookmark::toBookmark(QnCameraBookmark& bookmark) const {
    bookmark.guid = guid;
    bookmark.startTimeMs = startTime;
    bookmark.durationMs = duration;
    bookmark.name = name;
    bookmark.description = description;
    bookmark.colorIndex = colorIndex;
    bookmark.lockTime = lockTime;
    bookmark.tags.clear();
    for (const QString &tag: tags)
        bookmark.tags << tag;
}

void ApiCameraBookmark::fromBookmark(const QnCameraBookmark& bookmark) {
    guid = bookmark.guid.toByteArray();
    startTime = bookmark.startTimeMs;
    duration = bookmark.durationMs;
    name = bookmark.name;
    description = bookmark.description;
    colorIndex = bookmark.colorIndex;
    lockTime = bookmark.lockTime;
    tags.clear();
    tags.reserve(bookmark.tags.size());
    for (const QString &tag: bookmark.tags)
        tags.push_back(tag.trimmed());
}

void ApiCamera::toResource(QnVirtualCameraResourcePtr resource) const
{
	ApiResource::toResource(resource);

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

    QnCameraBookmarkList outBookmarks;
    for (int i = 0; i < bookmarks.size(); ++i) {
        outBookmarks << QnCameraBookmark();
        bookmarks[i].toBookmark(outBookmarks.last());
    }
    resource->setBookmarks(outBookmarks);
}


void ApiCamera::fromResource(const QnVirtualCameraResourcePtr& resource)
{
    ApiResource::fromResource(resource);

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

    const QnCameraBookmarkMap& cameraBookmarks = resource->getBookmarks();
    bookmarks.clear();
    bookmarks.reserve( cameraBookmarks.size() );
    for( const QnCameraBookmark& bookmark: cameraBookmarks )
    {
        bookmarks.push_back(ApiCameraBookmark());
        bookmarks.back().fromBookmark(bookmark);
    }
}

template <class T> 
void ApiCameraList::toResourceList(QList<T>& outData, QnResourceFactory* factory) const
{
    outData.reserve(outData.size() + data.size());
    for(int i = 0; i < data.size(); ++i) 
    {
        QnVirtualCameraResourcePtr camera = factory->createResource(data[i].typeId, QnResourceParams(data[i].url, data[i].vendor)).dynamicCast<QnVirtualCameraResource>();
        if (camera) {
            data[i].toResource(camera);
            outData << camera;
        }
    }
}

void ApiCameraList::fromResourceList(const QList<QnVirtualCameraResourcePtr>& cameras)
{
    data.resize(cameras.size());
    for(int i = 0; i < cameras.size(); ++i)
        data[i].fromResource(cameras[i]);
}


template void ApiCameraList::toResourceList<QnResourcePtr>(QList<QnResourcePtr>& outData, QnResourceFactory* factory) const;
template void ApiCameraList::toResourceList<QnVirtualCameraResourcePtr>(QList<QnVirtualCameraResourcePtr>& outData, QnResourceFactory* factory) const;


void ApiCameraList::loadFromQuery(QSqlQuery& query)
{
    QN_QUERY_TO_DATA_OBJECT(query, ApiCamera, data, apiCameraDataFields ApiResourceFields)
}

}
