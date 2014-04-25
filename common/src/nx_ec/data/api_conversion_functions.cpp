#include "api_conversion_functions.h"

#include <api/serializer/serializer.h>

#include <business/business_event_parameters.h>
#include <business/business_action_parameters.h>
#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_action_factory.h>

#include <core/misc/schedule_task.h>
#include <core/resource/camera_resource.h>

#include "api_business_rule_data.h"
#include "api_camera_data.h"

namespace ec2 {

struct overload_tag {};

#if 0
    void ApiBusinessRuleList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiBusinessRule, data, ApiBusinessRuleFields)
    }

    void ApiCameraList::loadFromQuery(QSqlQuery& query)
    {
        QN_QUERY_TO_DATA_OBJECT(query, ApiCamera, data, apiCameraDataFields ApiResourceFields)
    }


#endif

void fromApiToResource(const ApiBusinessRuleData &src, QnBusinessEventRulePtr &dst, QnResourcePool *) {
    dst->setId(src.id);
    dst->setEventType(src.eventType);

    dst->setEventResources(QVector<QnId>::fromStdVector(src.eventResource));

    dst->setEventParams(QnBusinessEventParameters::fromBusinessParams(deserializeBusinessParams(src.eventCondition)));

    dst->setEventState(src.eventState);
    dst->setActionType(src.actionType);

    dst->setActionResources(QVector<QnId>::fromStdVector(src.actionResource));

    dst->setActionParams(QnBusinessActionParameters::fromBusinessParams(deserializeBusinessParams(src.actionParams)));

    dst->setAggregationPeriod(src.aggregationPeriod);
    dst->setDisabled(src.disabled);
    dst->setComments(src.comments);
    dst->setSchedule(src.schedule);
    dst->setSystem(src.system);
}

void fromResourceToApi(const QnBusinessEventRulePtr &src, ApiBusinessRuleData &dst) {
    dst.id = src->id();
    dst.eventType = src->eventType();

    dst.eventResource = src->eventResources().toStdVector();
    dst.actionResource = src->actionResources().toStdVector();

    dst.eventCondition = serializeBusinessParams(src->eventParams().toBusinessParams());
    dst.actionParams = serializeBusinessParams(src->actionParams().toBusinessParams());

    dst.eventState = src->eventState();
    dst.actionType = src->actionType();
    dst.aggregationPeriod = src->aggregationPeriod();
    dst.disabled = src->disabled();
    dst.comments = src->comments();
    dst.schedule = src->schedule();
    dst.system = src->system();
}

void fromApiToResourceList(const ApiBusinessRuleDataList &src, QnBusinessEventRuleList &dst, QnResourcePool *resourcePool) {
    dst.reserve(dst.size() + src.size());
    for(int i = 0; i < src.size(); ++i) {
        dst.push_back(QnBusinessEventRulePtr(new QnBusinessEventRule()));
        fromApiToResource(src[i], dst.back(), resourcePool);
    }
}

void fromResourceListToApi(const QnBusinessEventRuleList &src, ApiBusinessRuleDataList &dst) {
    dst.reserve(dst.size() + src.size());
    foreach(const QnBusinessEventRulePtr &rule, src) {
        dst.push_back(ApiBusinessRuleData());
        fromResourceToApi(rule, dst.back());
    }
}

void fromResourceToApi(const QnAbstractBusinessActionPtr &src, ApiBusinessActionData &dst) {
    dst.actionType = src->actionType();
    dst.toggleState = src->getToggleState();
    dst.receivedFromRemoteHost = src->isReceivedFromRemoteHost();
    dst.resources = src->getResources().toStdVector();

    dst.params = serializeBusinessParams(src->getParams().toBusinessParams());
    dst.runtimeParams = serializeBusinessParams(src->getRuntimeParams().toBusinessParams());

    dst.businessRuleId = src->getBusinessRuleId();
    dst.aggregationCount = src->getAggregationCount();
}

void fromApiToResource(const ApiBusinessActionData &src, QnAbstractBusinessActionPtr &dst, QnResourcePool *) {
    dst = QnBusinessActionFactory::createAction(static_cast<QnBusiness::ActionType>(src.actionType), QnBusinessEventParameters::fromBusinessParams(deserializeBusinessParams(src.runtimeParams)));

    dst->setToggleState(src.toggleState);
    dst->setReceivedFromRemoteHost(src.receivedFromRemoteHost);

    dst->setResources(QVector<QnId>::fromStdVector(src.resources));

    dst->setParams(QnBusinessActionParameters::fromBusinessParams(deserializeBusinessParams(src.params)));

    dst->setBusinessRuleId(src.businessRuleId);
    dst->setAggregationCount(src.aggregationCount);
}

void fromResourceToApi(const QnScheduleTask &src, ApiScheduleTaskData &dst) {
    dst.startTime = src.getStartTime();
    dst.endTime = src.getEndTime();
    dst.doRecordAudio = src.getDoRecordAudio();
    dst.recordType = src.getRecordingType();
    dst.dayOfWeek = src.getDayOfWeek();
    dst.beforeThreshold = src.getBeforeThreshold();
    dst.afterThreshold = src.getAfterThreshold();
    dst.streamQuality = src.getStreamQuality();
    dst.fps = src.getFps();
}

void fromApiToResource(const ApiScheduleTaskData &src, QnScheduleTask &dst, const QnId &resourceId) {
    dst = QnScheduleTask(resourceId, src.dayOfWeek, src.startTime, src.endTime, src.recordType, src.beforeThreshold, src.afterThreshold, src.streamQuality, src.fps, src.doRecordAudio);
}

void fromApiToResource(const ApiCameraData &src, QnVirtualCameraResourcePtr &dst) {
    //fromApiToResource(*this, dst); // TODO: #Elric #EC2

    dst->setScheduleDisabled(src.scheduleDisabled);
    dst->setMotionType(src.motionType);

    QList<QnMotionRegion> regions;
    parseMotionRegionList(regions, src.region);
    dst->setMotionRegionList(regions, QnDomainMemory);

    dst->setMAC(src.mac);
    QAuthenticator auth;
    auth.setUser(src.login);
    auth.setPassword(src.password);
    dst->setAuth(auth);

    QnScheduleTaskList tasks;
    tasks.reserve(src.scheduleTask.size());
    foreach(const ApiScheduleTaskData &scheduleTask, src.scheduleTask) {
        tasks.push_back(QnScheduleTask());
        fromApiToResource(scheduleTask, tasks.back(), src.id);
    }
    dst->setScheduleTasks(tasks);

    dst->setAudioEnabled(src.audioEnabled);
    dst->setPhysicalId(src.physicalId);
    dst->setManuallyAdded(src.manuallyAdded);
    dst->setModel(src.model);
    dst->setFirmware(src.firmware);
    dst->setGroupId(src.groupId);
    dst->setGroupName(src.groupName);
    dst->setSecondaryStreamQuality(src.secondaryQuality);
    dst->setCameraControlDisabled(src.controlDisabled);
    dst->setStatusFlags(static_cast<QnSecurityCamResource::StatusFlags>(src.statusFlags));

    dst->setDewarpingParams(QJson::deserialized<QnMediaDewarpingParams>(src.dewarpingParams));
    dst->setVendor(src.vendor);
}


void fromResourceToApi(const QnVirtualCameraResourcePtr &src, ApiCameraData &dst) {
    //fromResourceToApi(resource, *this); // TODO: #Elric

    dst.scheduleDisabled = src->isScheduleDisabled();
    dst.motionType = src->getMotionType();

    QList<QnMotionRegion> regions;
    dst.region = serializeMotionRegionList(src->getMotionRegionList()).toLocal8Bit();
    dst.mac = src->getMAC().toString().toLocal8Bit();
    dst.login = src->getAuth().user();
    dst.password = src->getAuth().password();
    
    dst.scheduleTask.clear();
    foreach(QnScheduleTask task, src->getScheduleTasks()) {
        dst.scheduleTask.push_back(ApiScheduleTaskData());
        fromResourceToApi(task, dst.scheduleTask.back());
    }

    dst.audioEnabled = src->isAudioEnabled();
    dst.physicalId = src->getPhysicalId();
    dst.manuallyAdded = src->isManuallyAdded();
    dst.model = src->getModel();
    dst.firmware = src->getFirmware();
    dst.groupId = src->getGroupId();
    dst.groupName = src->getGroupName();
    dst.secondaryQuality = src->secondaryStreamQuality();
    dst.controlDisabled = src->isCameraControlDisabled();
    dst.statusFlags = src->statusFlags();
    dst.dewarpingParams = QJson::serialized<QnMediaDewarpingParams>(src->getDewarpingParams());
    dst.vendor = src->getVendor();
}

template<class List> 
void fromApiToResourceList(const ApiCameraDataList &src, List &dst, QnResourceFactory *factory, const overload_tag &) {
    dst.reserve(dst.size() + src.size());
    for(int i = 0; i < src.size(); ++i) {
        QnVirtualCameraResourcePtr camera = factory->createResource(src[i].typeId, QnResourceParams(src[i].url, src[i].vendor)).dynamicCast<QnVirtualCameraResource>();
        if (camera) {
            fromApiToResource(src, camera);
            dst.push_back(camera);
        }
    }
}

void fromApiToResourceList(const ApiCameraDataList &src, QnResourceList &dst, QnResourceFactory *factory) {
    fromApiToResourceList(src, dst, factory, overload_tag());
}

void fromApiToResourceList(const ApiCameraDataList &src, QnVirtualCameraResourceList &dst, QnResourceFactory *factory) {
    fromApiToResourceList(src, dst, factory, overload_tag());
}

void fromResourceListToApi(const QnVirtualCameraResourceList &src, ApiCameraDataList &dst) {
    dst.reserve(dst.size() + src.size());
    for(int i = 0; i < src.size(); ++i) {
        dst.push_back(ApiCameraData());
        fromResourceToApi(src[i], dst.back());
    }
}


} // namespace ec2
