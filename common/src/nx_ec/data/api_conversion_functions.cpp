#include "api_conversion_functions.h"

#include <api/serializer/serializer.h>

#include <business/business_event_parameters.h>
#include <business/business_action_parameters.h>
#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_action_factory.h>

#include <core/misc/schedule_task.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <nx_ec/ec_api.h>

#include "api_business_rule_data.h"
#include "api_camera_data.h"
#include "api_camera_server_item_data.h"
#include "api_email_data.h"
#include "api_full_info_data.h"
#include "api_layout_data.h"
#include "api_license_data.h"
#include "api_media_server_data.h"
#include "api_resource_data.h"
#include "api_resource_type_data.h"
#include "api_user_data.h"
#include "api_videowall_data.h"

namespace ec2 {

struct overload_tag {};

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
    for(const ApiBusinessRuleData &srcRule: src) {
        dst.push_back(QnBusinessEventRulePtr(new QnBusinessEventRule()));
        fromApiToResource(srcRule, dst.back(), resourcePool);
    }
}

void fromResourceListToApi(const QnBusinessEventRuleList &src, ApiBusinessRuleDataList &dst) {
    dst.reserve(dst.size() + src.size());
    for(const QnBusinessEventRulePtr &srcRule: src) {
        dst.push_back(ApiBusinessRuleData());
        fromResourceToApi(srcRule, dst.back());
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
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

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
    for(const ApiScheduleTaskData &srcTask: src.scheduleTask) {
        tasks.push_back(QnScheduleTask());
        fromApiToResource(srcTask, tasks.back(), src.id);
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
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    dst.scheduleDisabled = src->isScheduleDisabled();
    dst.motionType = src->getMotionType();

    QList<QnMotionRegion> regions;
    dst.region = serializeMotionRegionList(src->getMotionRegionList()).toLocal8Bit();
    dst.mac = src->getMAC().toString().toLocal8Bit();
    dst.login = src->getAuth().user();
    dst.password = src->getAuth().password();
    
    dst.scheduleTask.clear();
    for(const QnScheduleTask &srcTask: src->getScheduleTasks()) {
        dst.scheduleTask.push_back(ApiScheduleTaskData());
        fromResourceToApi(srcTask, dst.scheduleTask.back());
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
    for(const ApiCameraData &srcCamera: src) {
        QnVirtualCameraResourcePtr camera = factory->createResource(srcCamera.typeId, QnResourceParams(srcCamera.url, srcCamera.vendor)).dynamicCast<QnVirtualCameraResource>();
        if (camera) {
            fromApiToResource(srcCamera, camera);
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
    for(const QnVirtualCameraResourcePtr &srcCamera: src) {
        dst.push_back(ApiCameraData());
        fromResourceToApi(srcCamera, dst.back());
    }
}


void fromResourceToApi(const QnCameraHistoryItem &src, ApiCameraServerItemData &dst) {
    dst.physicalId = src.physicalId;
    dst.serverGuid = src.mediaServerGuid;
    dst.timestamp = src.timestamp;
}

void fromApiToResource(const ApiCameraServerItemData &src, QnCameraHistoryItem &dst) {
    dst.physicalId = src.physicalId;
    dst.mediaServerGuid = src.serverGuid;
    dst.timestamp = src.timestamp;
}

void fromApiToResourceList(const ApiCameraServerItemDataList &src, QnCameraHistoryList &dst) {
    /* CameraMAC -> (Timestamp -> ServerGuid). */
    QMap<QString, QMap<qint64, QString> > history;

    /* Fill temporary history map. */
    for (auto pos = src.begin(); pos != src.end(); ++pos)
        history[pos->physicalId][pos->timestamp] = pos->serverGuid;

    for(auto pos = history.begin(); pos != history.end(); ++pos) {
        QnCameraHistoryPtr cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());

        if (pos.value().isEmpty())
            continue;

        QMapIterator<qint64, QString> camit(pos.value());
        camit.toFront();

        qint64 duration;
        cameraHistory->setPhysicalId(pos.key());
        while (camit.hasNext())
        {
            camit.next();

            if (camit.hasNext())
                duration = camit.peekNext().key() - camit.key();
            else
                duration = -1;

            cameraHistory->addTimePeriod(QnCameraTimePeriod(camit.key(), duration, camit.value()));
        }

        dst.append(cameraHistory);
    }
}


void fromResourceToApi(const QnEmail::Settings &src, ApiEmailSettingsData &dst) {
    dst.host = src.server;
    dst.port = src.port;
    dst.user = src.user;
    dst.password = src.password;
    dst.connectionType = src.connectionType;
}

void fromApiToResource(const ApiEmailSettingsData &src, QnEmail::Settings &dst) {
    dst.server = src.host;
    dst.port = src.port;
    dst.user = src.user;
    dst.password = src.password;
    dst.connectionType = src.connectionType;
}


void fromApiToResourceList(const ApiFullInfoData &src, QnFullResourceData &dst, const ResourceContext &ctx) {
    fromApiToResourceList(src.resTypes, dst.resTypes);
    for(const QnResourceTypePtr &resType: dst.resTypes)
        const_cast<QnResourceTypePool*>(ctx.resTypePool)->addResourceType(resType); // TODO: #AK refactor it!

    fromApiToResourceList(src.servers, dst.resources, ctx);
    fromApiToResourceList(src.cameras, dst.resources, ctx.resFactory);
    fromApiToResourceList(src.users, dst.resources);
    fromApiToResourceList(src.layouts, dst.resources);
    fromApiToResourceList(src.videowalls, dst.resources);
    fromApiToResourceList(src.licenses, dst.licenses);
    fromApiToResourceList(src.rules, dst.bRules, ctx.pool);
    fromApiToResourceList(src.cameraHistory, dst.cameraHistory);

    dst.serverInfo = src.serverInfo;
}


void fromApiToResource(const ApiLayoutItemData &src, QnLayoutItemData &dst) {
    dst.uuid = src.uuid;
    dst.flags = src.flags;
    dst.combinedGeometry = QRectF(QPointF(src.left, src.top), QPointF(src.right, src.bottom));
    dst.rotation = src.rotation;
    dst.resource.id =  src.resourceId;
    dst.resource.path = src.resourcePath;
    dst.zoomRect = QRectF(QPointF(src.zoomLeft, src.zoomTop), QPointF(src.zoomRight, src.zoomBottom));
    dst.zoomTargetUuid = src.zoomTargetUuid;
    dst.contrastParams = ImageCorrectionParams::deserialize(src.contrastParams);
    dst.dewarpingParams = QJson::deserialized<QnItemDewarpingParams>(src.dewarpingParams);
}

void fromResourceToApi(const QnLayoutItemData &src, ApiLayoutItemData &dst) {
    dst.uuid = src.uuid.toByteArray();
    dst.flags = src.flags;
    dst.left = src.combinedGeometry.topLeft().x();
    dst.top = src.combinedGeometry.topLeft().y();
    dst.right = src.combinedGeometry.bottomRight().x();
    dst.bottom = src.combinedGeometry.bottomRight().y();
    dst.rotation = src.rotation;
    dst.resourceId = src.resource.id;
    dst.resourcePath = src.resource.path;
    dst.zoomLeft = src.zoomRect.topLeft().x();
    dst.zoomTop = src.zoomRect.topLeft().y();
    dst.zoomRight = src.zoomRect.bottomRight().x();
    dst.zoomBottom = src.zoomRect.bottomRight().y();
    dst.zoomTargetUuid = src.zoomTargetUuid.toByteArray();
    dst.contrastParams = src.contrastParams.serialize();
    dst.dewarpingParams = QJson::serialized(src.dewarpingParams);
}

void fromApiToResource(const ApiLayoutData &src, QnLayoutResourcePtr &dst) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    dst->setCellAspectRatio(src.cellAspectRatio);
    dst->setCellSpacing(src.cellSpacingWidth, src.cellSpacingHeight);
    dst->setUserCanEdit(src.userCanEdit);
    dst->setLocked(src.locked);
    dst->setBackgroundImageFilename(src.backgroundImageFilename);
    dst->setBackgroundSize(QSize(src.backgroundWidth, src.backgroundHeight));
    dst->setBackgroundOpacity(src.backgroundOpacity);
    
    QnLayoutItemDataList dstItems;
    for (const ApiLayoutItemData &srcItem: src.items) {
        dstItems.push_back(QnLayoutItemData());
        fromApiToResource(srcItem, dstItems.back());
    }
    dst->setItems(dstItems);
}

void fromResourceToApi(const QnLayoutResourcePtr &src, ApiLayoutData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    dst.cellAspectRatio = src->cellAspectRatio();
    dst.cellSpacingWidth = src->cellSpacing().width();
    dst.cellSpacingHeight = src->cellSpacing().height();
    dst.userCanEdit = src->userCanEdit();
    dst.locked = src->locked();
    dst.backgroundImageFilename = src->backgroundImageFilename();
    dst.backgroundWidth = src->backgroundSize().width();
    dst.backgroundHeight = src->backgroundSize().height();
    dst.backgroundOpacity = src->backgroundOpacity();
    
    const QnLayoutItemDataMap &srcItems = src->getItems();
    dst.items.clear();
    dst.items.reserve(srcItems.size());

    for(const QnLayoutItemData &item: srcItems) {
        dst.items.push_back(ApiLayoutItemData());
        fromResourceToApi(item, dst.items.back());
    }
}

template<class List>
void fromApiToResourceList(const ApiLayoutDataList &src, List &dst, const overload_tag &) {
    dst.reserve(dst.size() + src.size());
    for(const ApiLayoutData &srcLayout: src) {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        fromApiToResource(srcLayout, layout);
        dst.push_back(layout);
    }
}

void fromApiToResourceList(const ApiLayoutDataList &src, QnResourceList &dst) {
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const ApiLayoutDataList &src, QnLayoutResourceList &dst) {
    fromApiToResourceList(src, dst, overload_tag());
}

void fromResourceListToApi(const QnLayoutResourceList &src, ApiLayoutDataList &dst) {
    dst.reserve(dst.size() + src.size());
    for(const QnLayoutResourcePtr &layout: src) {
        dst.push_back(ApiLayoutData());
        fromResourceToApi(layout, dst.back());
    }
}

void fromResourceToApi(const QnLicensePtr &src, ApiLicenseData &dst) {
    dst.key = src->key();
    dst.licenseBlock = src->rawLicense();
}

void fromApiToResource(const ApiLicenseData &src, QnLicensePtr &dst) {
    dst->loadLicenseBlock(src.licenseBlock);
}

void fromResourceListToApi(const QnLicenseList &src, ApiLicenseDataList &dst) {
    dst.reserve(dst.size() + src.size());
    
    for(const QnLicensePtr &srcLicense: src) {
        dst.push_back(ApiLicenseData());
        fromResourceToApi(srcLicense, dst.back());
    }
}

void fromApiToResourceList(const ApiLicenseDataList &src, QnLicenseList &dst) {
    dst.reserve(dst.size() + src.size());
    for(const ApiLicenseData &srcLicense: src) {
        dst.push_back(QnLicensePtr(new QnLicense()));
        fromApiToResource(srcLicense, dst.back());
    }
}


static void deserializeNetAddrList(QList<QHostAddress>& netAddrList, const QString& netAddrListString) {
    QStringList addListStrings = netAddrListString.split(QLatin1Char(';'));
    std::transform(addListStrings.begin(), addListStrings.end(), std::back_inserter(netAddrList), [](const QString &address) { return QHostAddress(address); });
}

static QString serializeNetAddrList(const QList<QHostAddress>& netAddrList) {
    QStringList addListStrings;
    std::transform(netAddrList.begin(), netAddrList.end(), std::back_inserter(addListStrings), std::mem_fun_ref(&QHostAddress::toString));
    return addListStrings.join(QLatin1String(";"));
}

void fromResourceToApi(const QnAbstractStorageResourcePtr &src, ApiStorageData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    dst.spaceLimit = src->getSpaceLimit();
    dst.usedForWriting = src->isUsedForWriting();
}

void fromApiToResource(const ApiStorageData &src, QnAbstractStorageResourcePtr &dst) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    dst->setSpaceLimit(src.spaceLimit);
    dst->setUsedForWriting(src.usedForWriting);
}

void fromResourceToApi(const QnMediaServerResourcePtr& src, ApiMediaServerData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    dst.netAddrList = serializeNetAddrList(src->getNetAddrList());
    dst.apiUrl = src->getApiUrl();
    dst.flags = src->getServerFlags();
    dst.panicMode = src->getPanicMode();
    dst.streamingUrl = src->getStreamingUrl();
    dst.version = src->getVersion().toString();
    dst.maxCameras = src->getMaxCameras();
    dst.redundancy = src->isRedundancy();
    //authKey = resource->getAuthKey();

    QnAbstractStorageResourceList storageList = src->getStorages();
    dst.storages.resize(storageList.size());
    for (int i = 0; i < storageList.size(); ++i)
        fromResourceToApi(storageList[i], dst.storages[i]);
}

void fromApiToResource(const ApiMediaServerData &src, QnMediaServerResourcePtr &dst, const ResourceContext &ctx) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    QList<QHostAddress> resNetAddrList;
    deserializeNetAddrList(resNetAddrList, src.netAddrList);

    dst->setApiUrl(src.apiUrl);
    dst->setNetAddrList(resNetAddrList);
    dst->setServerFlags(src.flags);
    dst->setPanicMode(static_cast<Qn::PanicMode>(src.panicMode));
    dst->setStreamingUrl(src.streamingUrl);
    dst->setVersion(QnSoftwareVersion(src.version));
    dst->setMaxCameras(src.maxCameras);
    dst->setRedundancy(src.redundancy);
    //dst->setAuthKey(authKey);

    QnResourceTypePtr resType = ctx.resTypePool->getResourceTypeByName(lit("Storage"));
    if (!resType)
        return;

    QnAbstractStorageResourceList dstStorages;
    for(const ApiStorageData &srcStorage: src.storages) {
        QnAbstractStorageResourcePtr dstStorage = ctx.resFactory->createResource(resType->getId(), QnResourceParams(srcStorage.url, QString())).dynamicCast<QnAbstractStorageResource>();

        fromApiToResource(srcStorage, dstStorage);
        dstStorages.push_back(dstStorage);
    }

    dst->setStorages(dstStorages);
}

template<class List> 
void fromApiToResourceList(const ApiMediaServerDataList &src, List &dst, const ResourceContext &ctx, const overload_tag &) {
    dst.reserve(dst.size() + src.size());
    for(const ApiMediaServerData &srcServer: src) {
        QnMediaServerResourcePtr dstElement(new QnMediaServerResource(ctx.resTypePool));
        fromApiToResource(srcServer, dstElement, ctx);
        dst.push_back(std::move(dstElement));
    }
}

void fromApiToResourceList(const ApiMediaServerDataList &src, QnResourceList &dst, const ResourceContext &ctx) {
    fromApiToResourceList(src, dst, ctx, overload_tag());
}

void fromApiToResourceList(const ApiMediaServerDataList &src, QnMediaServerResourceList &dst, const ResourceContext &ctx) {
    fromApiToResourceList(src, dst, ctx, overload_tag());
}


void fromResourceToApi(const QnResourcePtr &src, ApiResourceData &dst) {
    Q_ASSERT(!src->getId().isNull());
    Q_ASSERT(!src->getTypeId().isNull());

    dst.id = src->getId();
    dst.typeId = src->getTypeId();
    dst.parentGuid = src->getParentId();
    dst.name = src->getName();
    dst.url = src->getUrl();
    dst.status = src->getStatus();

    QnParamList params = src->getResourceParamList();
    for(const QnParam &srcParam: src->getResourceParamList().list())
        if (srcParam.domain() == QnDomainDatabase)
            dst.addParams.push_back(ApiResourceParamData(srcParam.name(), srcParam.value().toString(), true));
}

void fromApiToResource(const ApiResourceData &src, QnResourcePtr &dst) {
    dst->setId(src.id);
    //dst->setGuid(guid);
    dst->setTypeId(src.typeId);
    dst->setParentId(src.parentGuid);
    dst->setName(src.name);
    dst->setUrl(src.url);
    dst->setStatus(src.status, true);

    for(const ApiResourceParamData &srcParam: src.addParams) {
        if (srcParam.isResTypeParam)
            dst->setParam(srcParam.name, srcParam.value, QnDomainDatabase);
        else
            dst->setProperty(srcParam.name, srcParam.value);
    }
}

void fromApiToResourceList(const ApiResourceDataList &src, QnResourceList &dst, QnResourceFactory *factory) {
    dst.reserve(dst.size() + src.size());
    for(const ApiResourceData &srcResource: src) {
        dst.push_back(factory->createResource(srcResource.typeId, QnResourceParams(srcResource.url, QString())));
        fromApiToResource(srcResource, dst.back());
    }
}

void fromResourceListToApi(const QnKvPairList &src, ApiResourceParamDataList &dst) {
    dst.resize(dst.size() + src.size());
    for (const QnKvPair &srcParam: src)
        dst.push_back(ApiResourceParamData(srcParam.name(), srcParam.value(), false));
}

void fromApiToResourceList(const ApiResourceParamDataList &src, QnKvPairList &dst) {
    dst.reserve(dst.size() + src.size());

    for(const ApiResourceParamData &srcParam: src)
        dst.push_back(QnKvPair(srcParam.name, srcParam.value));
}


void fromApiToResource(const ApiPropertyTypeData &src, QnParamTypePtr &dst) {
    //resource->id = id;
    dst->name = src.name;
    dst->type = (QnParamType::DataType) src.type;
    dst->min_val = src.min;
    dst->max_val = src.max;
    dst->step = src.step;
    foreach(const QString &val, src.values.split(QLatin1Char(',')))
        dst->possible_values << val.trimmed();
    foreach(const QString &val, src.ui_values.split(QLatin1Char(',')))
        dst->ui_possible_values << val.trimmed();
    dst->default_value = src.default_value;
    dst->group = src.group;
    dst->subgroup = src.sub_group;
    dst->description = src.description;
    dst->ui = src.ui;
    dst->isReadOnly = src.readonly;
    dst->paramNetHelper = src.netHelper;
}


void fromApiToResource(const ApiResourceTypeData &src, QnResourceTypePtr &dst) {
    dst->setId(src.id);
    dst->setName(src.name);
    dst->setManufacture(src.manufacture);

    if (!src.parentId.empty())
        dst->setParentId(src.parentId[0]);
    for (int i = 1; i < src.parentId.size(); ++i)
        dst->addAdditionalParent(src.parentId[i]);

    for(const ApiPropertyTypeData &p: src.propertyTypeList) {
        QnParamTypePtr param(new QnParamType());
        fromApiToResource(p, param);
        dst->addParamType(param);
    }
}

void fromApiToResourceList(const ApiResourceTypeDataList &src, QnResourceTypeList &dst) {
    dst.reserve(src.size() + dst.size());

    for(const ApiResourceTypeData &srcType: src) {
        dst.push_back(QnResourceTypePtr(new QnResourceType()));
        fromApiToResource(srcType, dst.back());
    }
}


void fromApiToResource(const ApiUserData &src, QnUserResourcePtr &dst) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    dst->setAdmin(src.isAdmin);
    dst->setEmail(src.email);
    dst->setHash(src.hash);

    dst->setPermissions(src.rights);
    dst->setDigest(src.digest);
}

void fromResourceToApi(const QnUserResourcePtr &src, ApiUserData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    QString password = src->getPassword();

    if (!password.isEmpty()) {
        QByteArray salt = QByteArray::number(rand(), 16);
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData(salt);
        md5.addData(password.toUtf8());
        dst.hash = "md5$";
        dst.hash.append(salt);
        dst.hash.append("$");
        dst.hash.append(md5.result().toHex());

        md5.reset();
        md5.addData(QString(lit("%1:NetworkOptix:%2")).arg(src->getName(), password).toLatin1());
        dst.digest = md5.result().toHex();
    }

    dst.isAdmin = src->isAdmin();
    dst.rights = src->getPermissions();
    dst.email = src->getEmail();
}

template<class List>
void fromApiToResourceList(const ApiUserDataList &src, List &dst, const overload_tag &) {
    dst.reserve(dst.size() + src.size());

    for(const ApiUserData &srcUser: src) {
        dst.push_back(QnUserResourcePtr(new QnUserResource()));
        fromApiToResource(srcUser, dst.back());
    }
}

void fromApiToResourceList(const ApiUserDataList &src, QnResourceList &dst) {
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const ApiUserDataList &src, QnUserResourceList &dst) {
    fromApiToResourceList(src, dst, overload_tag());
}


void fromApiToResource(const ApiVideowallItemData &src, QnVideoWallItem &dst) {
    dst.uuid       = src.guid;
    dst.layout     = src.layout_guid;
    dst.pcUuid     = src.pc_guid;
    dst.name       = src.name;
    dst.geometry   = QRect(src.x, src.y, src.w, src.h);
}

void fromResourceToApi(const QnVideoWallItem &src, ApiVideowallItemData &dst) {
    dst.guid        = src.uuid;
    dst.layout_guid = src.layout;
    dst.pc_guid     = src.pcUuid;
    dst.name        = src.name;
    dst.x           = src.geometry.x();
    dst.y           = src.geometry.y();
    dst.w           = src.geometry.width();
    dst.h           = src.geometry.height();
}

void fromApiToResource(const ApiVideowallScreenData &src, QnVideoWallPcData::PcScreen &dst) {
    dst.index            = src.pc_index;
    dst.desktopGeometry  = QRect(src.desktop_x, src.desktop_y, src.desktop_w, src.desktop_h);
    dst.layoutGeometry   = QRect(src.layout_x, src.layout_y, src.layout_w, src.layout_h);
}

void fromResourceToApi(const QnVideoWallPcData::PcScreen &src, ApiVideowallScreenData &dst) {
    dst.pc_index    = src.index;
    dst.desktop_x   = src.desktopGeometry.x();
    dst.desktop_y   = src.desktopGeometry.y();
    dst.desktop_w   = src.desktopGeometry.width();
    dst.desktop_h   = src.desktopGeometry.height();
    dst.layout_x    = src.layoutGeometry.x();
    dst.layout_y    = src.layoutGeometry.y();
    dst.layout_w    = src.layoutGeometry.width();
    dst.layout_h    = src.layoutGeometry.height();
}

void fromApiToResource(const ApiVideowallData &src, QnVideoWallResourcePtr &dst) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    dst->setAutorun(src.autorun);
    QnVideoWallItemList outItems;
    for (const ApiVideowallItemData &item: src.items) {
        outItems << QnVideoWallItem();
        fromApiToResource(item, outItems.last());
    }
    dst->setItems(outItems);

    QnVideoWallPcDataMap pcs;
    for (const ApiVideowallScreenData &screen: src.screens) {
        QnVideoWallPcData::PcScreen outScreen;
        fromApiToResource(screen, outScreen);
        QnVideoWallPcData& outPc = pcs[screen.pc_guid];
        outPc.uuid = screen.pc_guid;
        outPc.screens << outScreen;
    }
    dst->setPcs(pcs);

}

void fromResourceToApi(const QnVideoWallResourcePtr &src, ApiVideowallData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    dst.autorun = src->isAutorun();

    const QnVideoWallItemMap& resourceItems = src->getItems();
    dst.items.clear();
    dst.items.reserve(resourceItems.size());
    for (const QnVideoWallItem &item: resourceItems) {
        ApiVideowallItemData itemData;
        fromResourceToApi(item, itemData);
        dst.items.push_back(itemData);
    }

    dst.screens.clear();
    for (const QnVideoWallPcData &pc: src->getPcs()) {
        for (const QnVideoWallPcData::PcScreen &screen: pc.screens) {
            ApiVideowallScreenData screenData;
            fromResourceToApi(screen, screenData);
            screenData.pc_guid = pc.uuid;
            dst.screens.push_back(screenData);
        }
    }
}

template<class List>
void fromApiToResourceList(const ApiVideowallDataList &src, List &dst, const overload_tag &) {
    dst.reserve(dst.size() + src.size());

    for(const ApiVideowallData &srcVideowall: src) {
        dst.push_back(QnVideoWallResourcePtr(new QnVideoWallResource()));
        fromApiToResource(srcVideowall, dst.back());
    }
}

void fromApiToResourceList(const ApiVideowallDataList &src, QnResourceList &dst) {
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const ApiVideowallDataList &src, QnVideoWallResourceList &dst) {
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResource(const ApiVideowallControlMessageData &data, QnVideoWallControlMessage &message) {
    message.operation = static_cast<QnVideoWallControlMessage::QnVideoWallControlOperation>(data.operation);
    message.videoWallGuid = data.videowall_guid;
    message.instanceGuid = data.instance_guid;
    message.params.clear();
    for (const std::pair<QString, QString> &pair : data.params)
        message.params[pair.first] = pair.second;
}

void fromResourceToApi(const QnVideoWallControlMessage &message, ApiVideowallControlMessageData &data) {
    data.operation = static_cast<int>(message.operation);
    data.videowall_guid = message.videoWallGuid;
    data.instance_guid = message.instanceGuid;
    data.params.clear();
    auto iter = message.params.constBegin();

    while (iter != message.params.constEnd()) {
        data.params.insert(std::pair<QString, QString>(iter.key(), iter.value()));
        ++iter;
    }
}

} // namespace ec2
