#include "api_conversion_functions.h"

#include <api/serializer/serializer.h>

#include <utils/serialization/json.h>

#include <business/business_event_parameters.h>
#include <business/business_action_parameters.h>
#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_action_factory.h>

#include <core/misc/schedule_task.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/misc/screen_snap.h>

#include <nx_ec/ec_api.h>
#include <network/authenticate_helper.h>

#include "api_business_rule_data.h"
#include "api_camera_data.h"
#include "api_camera_attributes_data.h"
#include "api_camera_data_ex.h"
#include "api_camera_bookmark_data.h"
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
#include "api_peer_data.h"
#include "api_runtime_data.h"

namespace ec2 {

struct overload_tag {};

void fromApiToResource(const ApiBusinessRuleData &src, QnBusinessEventRulePtr &dst, QnResourcePool *) {
    dst->setId(src.id);
    dst->setEventType(src.eventType);

    dst->setEventResources(QVector<QnUuid>::fromStdVector(src.eventResourceIds));

    dst->setEventParams(QnBusinessEventParameters::fromBusinessParams(deserializeBusinessParams(src.eventCondition)));

    dst->setEventState(src.eventState);
    dst->setActionType(src.actionType);

    dst->setActionResources(QVector<QnUuid>::fromStdVector(src.actionResourceIds));

    dst->setActionParams(QnBusinessActionParameters::fromBusinessParams(deserializeBusinessParams(src.actionParams)));

    dst->setAggregationPeriod(src.aggregationPeriod);
    dst->setDisabled(src.disabled);
    dst->setComment(src.comment);
    dst->setSchedule(src.schedule);
    dst->setSystem(src.system);
}

void fromResourceToApi(const QnBusinessEventRulePtr &src, ApiBusinessRuleData &dst) {
    dst.id = src->id();
    dst.eventType = src->eventType();

    dst.eventResourceIds = src->eventResources().toStdVector();
    dst.actionResourceIds = src->actionResources().toStdVector();

    dst.eventCondition = serializeBusinessParams(src->eventParams().toBusinessParams());
    dst.actionParams = serializeBusinessParams(src->actionParams().toBusinessParams());

    dst.eventState = src->eventState();
    dst.actionType = src->actionType();
    dst.aggregationPeriod = src->aggregationPeriod();
    dst.disabled = src->isDisabled();
    dst.comment = src->comment();
    dst.schedule = src->schedule();
    dst.system = src->isSystem();
}

void fromApiToResourceList(const ApiBusinessRuleDataList &src, QnBusinessEventRuleList &dst, QnResourcePool *resourcePool) {
    dst.reserve(dst.size() + (int)src.size());
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
    dst.resourceIds = src->getResources().toStdVector();

    dst.params = serializeBusinessParams(src->getParams().toBusinessParams());
    dst.runtimeParams = serializeBusinessParams(src->getRuntimeParams().toBusinessParams());

    dst.ruleId = src->getBusinessRuleId();
    dst.aggregationCount = src->getAggregationCount();
}

void fromApiToResource(const ApiBusinessActionData &src, QnAbstractBusinessActionPtr &dst, QnResourcePool *) {
    dst = QnBusinessActionFactory::createAction(static_cast<QnBusiness::ActionType>(src.actionType), QnBusinessEventParameters::fromBusinessParams(deserializeBusinessParams(src.runtimeParams)));

    dst->setToggleState(src.toggleState);
    dst->setReceivedFromRemoteHost(src.receivedFromRemoteHost);

    dst->setResources(QVector<QnUuid>::fromStdVector(src.resourceIds));

    dst->setParams(QnBusinessActionParameters::fromBusinessParams(deserializeBusinessParams(src.params)));

    dst->setBusinessRuleId(src.ruleId);
    dst->setAggregationCount(src.aggregationCount);
}


////////////////////////////////////////////////////////////
//// ApiCameraData
////////////////////////////////////////////////////////////

void fromApiToResource(const ApiCameraData &src, QnVirtualCameraResourcePtr &dst) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    { // test if the camera is desktop camera
        auto resType = qnResTypePool->desktopCameraResourceType();
        if (resType && resType->getId() == src.typeId)
            dst->addFlags(Qn::desktop_camera);
    }

    dst->setMAC(QnMacAddress(src.mac));

    dst->setPhysicalId(src.physicalId);
    dst->setManuallyAdded(src.manuallyAdded);
    dst->setModel(src.model);
    dst->setGroupId(src.groupId);
    dst->setGroupName(src.groupName);
    dst->setStatusFlags(src.statusFlags);

    dst->setVendor(src.vendor);
    Q_ASSERT(dst->getId() == QnVirtualCameraResource::uniqueIdToId(dst->getUniqueId()));
}


void fromResourceToApi(const QnVirtualCameraResourcePtr &src, ApiCameraData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    dst.mac = src->getMAC().toString().toLatin1();
    dst.physicalId = src->getPhysicalId();
    dst.manuallyAdded = src->isManuallyAdded();
    dst.model = src->getModel();
    dst.groupId = src->getGroupId();
    dst.groupName = src->getGroupName();
    dst.statusFlags = src->statusFlags();
    dst.vendor = src->getVendor();
}

template<class List> 
void fromApiToResourceList(const ApiCameraDataList &src, List &dst, QnResourceFactory *factory, const overload_tag &) {
    dst.reserve(dst.size() + (int)src.size());
    for(const ApiCameraData &srcCamera: src) {
        QnVirtualCameraResourcePtr dstCamera = factory->createResource(srcCamera.typeId, QnResourceParams(srcCamera.id, srcCamera.url, srcCamera.vendor)).dynamicCast<QnVirtualCameraResource>();
        if (dstCamera) {
            fromApiToResource(srcCamera, dstCamera);
            dst.push_back(dstCamera);
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


////////////////////////////////////////////////////////////
//// ApiCameraAttributesData
////////////////////////////////////////////////////////////

void fromResourceToApi(const QnScheduleTask &src, ApiScheduleTaskData &dst) {
    dst.startTime = src.getStartTime();
    dst.endTime = src.getEndTime();
    dst.recordAudio = src.getDoRecordAudio();
    dst.recordingType = src.getRecordingType();
    dst.dayOfWeek = src.getDayOfWeek();
    dst.beforeThreshold = src.getBeforeThreshold();
    dst.afterThreshold = src.getAfterThreshold();
    dst.streamQuality = src.getStreamQuality();
    dst.fps = src.getFps();
}

void fromApiToResource(const ApiScheduleTaskData &src, QnScheduleTask &dst, const QnUuid &resourceId) {
    dst = QnScheduleTask(resourceId, src.dayOfWeek, src.startTime, src.endTime, src.recordingType, 
                         src.beforeThreshold, src.afterThreshold, src.streamQuality, src.fps, src.recordAudio);
}

void fromApiToResource(const ApiCameraAttributesData &src, const QnCameraUserAttributesPtr& dst)
{
    dst->cameraID = src.cameraID;
    dst->name = src.cameraName;
    dst->scheduleDisabled = !src.scheduleEnabled;
    dst->motionType = src.motionType;

    QList<QnMotionRegion> regions;
    parseMotionRegionList(regions, src.motionMask);
    dst->motionRegions = regions;

    QnScheduleTaskList tasks;
    tasks.reserve((int)src.scheduleTasks.size());
    for(const ApiScheduleTaskData &srcTask: src.scheduleTasks) {
        tasks.push_back(QnScheduleTask());
        fromApiToResource(srcTask, tasks.back(), src.cameraID);
    }
    dst->scheduleTasks = tasks;

    dst->audioEnabled = src.audioEnabled;

    dst->secondaryQuality = src.secondaryStreamQuality;
    dst->cameraControlDisabled = !src.controlEnabled;
    dst->dewarpingParams = QJson::deserialized<QnMediaDewarpingParams>(src.dewarpingParams);
    dst->minDays = src.minArchiveDays;
    dst->maxDays = src.maxArchiveDays;
    dst->preferedServerId = src.preferedServerId;
}

void fromResourceToApi(const QnCameraUserAttributesPtr& src, ApiCameraAttributesData& dst)
{
    dst.cameraID = src->cameraID;
    dst.cameraName = src->name;
    dst.scheduleEnabled = !src->scheduleDisabled;
    dst.motionType = src->motionType;

    QList<QnMotionRegion> regions;
    dst.motionMask = serializeMotionRegionList(src->motionRegions).toLatin1();
    
    dst.scheduleTasks.clear();
    for(const QnScheduleTask &srcTask: src->scheduleTasks) {
        dst.scheduleTasks.push_back(ApiScheduleTaskData());
        fromResourceToApi(srcTask, dst.scheduleTasks.back());
    }

    dst.audioEnabled = src->audioEnabled;
    dst.secondaryStreamQuality = src->secondaryQuality;
    dst.controlEnabled = !src->cameraControlDisabled;
    dst.dewarpingParams = QJson::serialized<QnMediaDewarpingParams>(src->dewarpingParams);
    dst.minArchiveDays = src->minDays;
    dst.maxArchiveDays = src->maxDays;
    dst.preferedServerId = src->preferedServerId;
}

void fromApiToResourceList(const ApiCameraAttributesDataList& src, QnCameraUserAttributesList& dst)
{
    dst.reserve( dst.size()+src.size() );
    for( const ApiCameraAttributesData& cameraAttrs: src )
    {
        QnCameraUserAttributesPtr dstElement( new QnCameraUserAttributes() );
        fromApiToResource( cameraAttrs, dstElement );
        dst.push_back( std::move(dstElement) );
    }
}

void fromResourceListToApi(const QnCameraUserAttributesList& src, ApiCameraAttributesDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for(const QnCameraUserAttributesPtr& camerAttrs: src) {
        dst.push_back(ApiCameraAttributesData());
        fromResourceToApi(camerAttrs, dst.back());
    }
}


////////////////////////////////////////////////////////////
//// ApiCameraDataEx
////////////////////////////////////////////////////////////

void fromApiToResource(const ApiCameraDataEx& src, QnVirtualCameraResourcePtr& dst)
{
    fromApiToResource( static_cast<const ApiCameraData&>(src), dst );
    //TODO #ak using QnCameraUserAttributePool here is not good
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), dst->getId() );
    fromApiToResource( static_cast<const ApiCameraAttributesData&>(src), *userAttributesLock );

    for(const ApiResourceParamData &srcParam: src.addParams)
        dst->setProperty(srcParam.name, srcParam.value, false);
}

void fromResourceToApi(const QnVirtualCameraResourcePtr& src, ApiCameraDataEx& dst)
{
    fromResourceToApi( src, static_cast<ApiCameraData&>(dst) );
    //TODO #ak using QnCameraUserAttributePool here is not good
    QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), src->getId() );
    fromResourceToApi( *userAttributesLock, static_cast<ApiCameraAttributesData&>(dst) );

    for(const ec2::ApiResourceParamData &srcParam: src->getProperties())
        dst.addParams.push_back(srcParam);
}

void fromResourceListToApi(const QnVirtualCameraResourceList &src, ApiCameraDataExList &dst)
{
    dst.reserve(dst.size() + src.size());
    for(const QnVirtualCameraResourcePtr &srcCamera: src) {
        dst.push_back(ApiCameraDataEx());
        fromResourceToApi(srcCamera, dst.back());
    }
}


////////////////////////////////////////////////////////////
//// QnCameraHistoryItem
////////////////////////////////////////////////////////////

void fromResourceToApi(const QnCameraHistoryItem &src, ApiCameraServerItemData &dst) {
    dst.cameraUniqueId = src.cameraUniqueId;
    dst.serverId = src.mediaServerGuid.toString().toLatin1();
    dst.timestamp = src.timestamp;
}

void fromApiToResource(const ApiCameraServerItemData &src, QnCameraHistoryItem &dst) {
    dst.cameraUniqueId = src.cameraUniqueId;
    dst.mediaServerGuid = QnUuid(src.serverId);
    dst.timestamp = src.timestamp;
}

void fromApiToResourceList(const ApiCameraServerItemDataList &src, QnCameraHistoryList &dst) 
{
    /* CameraUniqueId -> (Timestamp -> ServerGuid). */
    QMap<QString, QMap<qint64, QByteArray> > history;

    /* Fill temporary history map. */
    for (auto pos = src.begin(); pos != src.end(); ++pos)
        history[pos->cameraUniqueId][pos->timestamp] = pos->serverId;

    for(auto pos = history.begin(); pos != history.end(); ++pos) {
        QnCameraHistoryPtr cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());

        if (pos.value().isEmpty())
            continue;

        QMapIterator<qint64, QByteArray> camit(pos.value());
        camit.toFront();

        qint64 duration;
        cameraHistory->setCameraUniqueId(pos.key());
        while (camit.hasNext())
        {
            camit.next();
            cameraHistory->addTimePeriod(camit.key(), camit.value());
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
    fromApiToResourceList(src.resourceTypes, dst.resTypes);
    for(const QnResourceTypePtr &resType: dst.resTypes)
        const_cast<QnResourceTypePool*>(ctx.resTypePool)->addResourceType(resType); // TODO: #AK refactor it!

    fromApiToResourceList(src.servers, dst.resources, ctx);
    fromApiToResourceList(src.serversUserAttributesList, dst.serverUserAttributesList);
    fromApiToResourceList(src.cameras, dst.resources, ctx.resFactory);
    fromApiToResourceList(src.cameraUserAttributesList, dst.cameraUserAttributesList);
    fromApiToResourceList(src.users, dst.resources);
    fromApiToResourceList(src.layouts, dst.resources, ctx);
    fromApiToResourceList(src.videowalls, dst.resources);
    fromApiToResourceList(src.licenses, dst.licenses);
    fromApiToResourceList(src.rules, dst.bRules, ctx.pool);
    fromApiToResourceList(src.cameraHistory, dst.cameraHistory);
    dst.allProperties = src.allProperties;
}


void fromApiToResource(const ApiLayoutItemData &src, QnLayoutItemData &dst) {
    dst.uuid = src.id;
    dst.flags = src.flags;
    dst.combinedGeometry = QRectF(QPointF(src.left, src.top), QPointF(src.right, src.bottom));
    dst.rotation = src.rotation;
    dst.resource.id =  src.resourceId;
    dst.resource.path = src.resourcePath;
    dst.zoomRect = QRectF(QPointF(src.zoomLeft, src.zoomTop), QPointF(src.zoomRight, src.zoomBottom));
    dst.zoomTargetUuid = src.zoomTargetId;
    dst.contrastParams = ImageCorrectionParams::deserialize(src.contrastParams);
    dst.dewarpingParams = QJson::deserialized<QnItemDewarpingParams>(src.dewarpingParams);
}

void fromResourceToApi(const QnLayoutItemData &src, ApiLayoutItemData &dst) {
    dst.id = src.uuid.toByteArray();
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
    dst.zoomTargetId = src.zoomTargetUuid.toByteArray();
    dst.contrastParams = src.contrastParams.serialize();
    dst.dewarpingParams = QJson::serialized(src.dewarpingParams);
}

void fromApiToResource(const ApiLayoutData &src, QnLayoutResourcePtr &dst) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    dst->setCellAspectRatio(src.cellAspectRatio);
    dst->setCellSpacing(src.horizontalSpacing, src.verticalSpacing);
    dst->setUserCanEdit(src.editable);
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
    dst.horizontalSpacing = src->cellSpacing().width();
    dst.verticalSpacing = src->cellSpacing().height();
    dst.editable = src->userCanEdit();
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
void fromApiToResourceList(const ApiLayoutDataList &src, List &dst, const ResourceContext &ctx, const overload_tag &) {
    dst.reserve(dst.size() + (int)src.size());
    for(const ApiLayoutData &srcLayout: src) {
        QnLayoutResourcePtr dstLayout(new QnLayoutResource(ctx.resTypePool));
        fromApiToResource(srcLayout, dstLayout);
        dst.push_back(dstLayout);
    }
}

void fromApiToResourceList(const ApiLayoutDataList &src, QnResourceList &dst, const ResourceContext &ctx) {
    fromApiToResourceList(src, dst, ctx, overload_tag());
}

void fromApiToResourceList(const ApiLayoutDataList &src, QnLayoutResourceList &dst, const ResourceContext &ctx) {
    fromApiToResourceList(src, dst, ctx, overload_tag());
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
    if (dst->key().isEmpty())
        dst->setKey(src.key);
}

void fromResourceToApi(const QnLicensePtr &src, ApiDetailedLicenseData &dst) {
    dst.key = src->key();
    dst.licenseBlock = src->rawLicense();
    dst.name = src->name();
    dst.cameraCount = src->cameraCount();
    dst.hardwareId = src->hardwareId();
    dst.licenseType = src->displayName();
    dst.version = src->version();
    dst.brand = src->brand();
    dst.expiration = src->expiration();

}


void fromResourceListToApi(const QnLicenseList &src, ApiLicenseDataList &dst) {
    dst.reserve(dst.size() + src.size());
    
    for(const QnLicensePtr &srcLicense: src) {
        dst.push_back(ApiLicenseData());
        fromResourceToApi(srcLicense, dst.back());
    }
}

void fromApiToResourceList(const ApiLicenseDataList &src, QnLicenseList &dst) {
    dst.reserve(dst.size() + (int)src.size());
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

void fromResourceToApi(const QnAbstractStorageResourceList &src, ApiStorageDataList &dst)
{
    foreach(const QnAbstractStorageResourcePtr& storage, src) 
    {
        ApiStorageData dstStorage;
        fromResourceToApi(storage, dstStorage);
        dst.push_back(std::move(dstStorage));
    }
}

void fromApiToResource(const ApiStorageData &src, QnAbstractStorageResourcePtr &dst) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    dst->setSpaceLimit(src.spaceLimit);
    dst->setUsedForWriting(src.usedForWriting);
}

void fromResourceToApi(const QnMediaServerResourcePtr& src, ApiMediaServerData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    dst.networkAddresses = serializeNetAddrList(src->getNetAddrList());
    dst.apiUrl = src->getApiUrl();
    dst.flags = src->getServerFlags();
    dst.panicMode = src->getPanicMode();
    dst.version = src->getVersion().toString();
    dst.systemInfo = src->getSystemInfo().toString();
    dst.authKey = src->getAuthKey();
    dst.systemName = src->getSystemName();

    QnAbstractStorageResourceList storageList = src->getStorages();
    dst.storages.resize(storageList.size());
    for (int i = 0; i < storageList.size(); ++i)
        fromResourceToApi(storageList[i], dst.storages[i]);
}

void fromApiToResource(const ApiMediaServerData &src, QnMediaServerResourcePtr &dst, const ResourceContext &ctx) {
    QnResourcePtr tmp = dst;
    fromApiToResource(static_cast<const ApiResourceData &>(src), tmp);

    QList<QHostAddress> resNetAddrList;
    deserializeNetAddrList(resNetAddrList, src.networkAddresses);

    dst->setApiUrl(src.apiUrl);
    dst->setNetAddrList(resNetAddrList);
    dst->setServerFlags(src.flags);
    dst->setPanicMode(src.panicMode);
    dst->setVersion(QnSoftwareVersion(src.version));
    dst->setSystemInfo(QnSystemInformation(src.systemInfo));
    dst->setAuthKey(src.authKey);
    dst->setSystemName(src.systemName);

    /*
    QnResourceTypePtr resType = ctx.resTypePool->getResourceTypeByName(lit("Storage"));
    if (!resType)
        return;

    QnAbstractStorageResourceList dstStorages;
    for(const ApiStorageData &srcStorage: src.storages) {
        QnAbstractStorageResourcePtr dstStorage = ctx.resFactory->createResource(resType->getId(), QnResourceParams(srcStorage.id, srcStorage.url, QString())).dynamicCast<QnAbstractStorageResource>();

        fromApiToResource(srcStorage, dstStorage);
        dstStorages.push_back(dstStorage);
    }
    dst->setStorages(dstStorages);
    */
}

template<class List> 
void fromApiToResourceList(const ApiMediaServerDataList &src, List &dst, const ResourceContext &ctx, const overload_tag &) {
    dst.reserve(dst.size() + (int)src.size());
    for(const ApiMediaServerData &srcServer: src) {
        QnMediaServerResourcePtr dstServer(new QnMediaServerResource(ctx.resTypePool));
        fromApiToResource(srcServer, dstServer, ctx);
        dst.push_back(std::move(dstServer));
    }
}

void fromApiToResourceList(const ApiMediaServerDataList &src, QnResourceList &dst, const ResourceContext &ctx) {
    fromApiToResourceList(src, dst, ctx, overload_tag());
}

void fromApiToResourceList(const ApiMediaServerDataList &src, QnMediaServerResourceList &dst, const ResourceContext &ctx) {
    fromApiToResourceList(src, dst, ctx, overload_tag());
}



////////////////////////////////////////////////////////////
//// ApiMediaServerUserAttributesData
////////////////////////////////////////////////////////////
void fromResourceToApi(const QnMediaServerUserAttributesPtr& src, ApiMediaServerUserAttributesData& dst) {
    dst.serverID = src->serverID;
    dst.serverName = src->name;
    dst.maxCameras = src->maxCameras;
    dst.allowAutoRedundancy = src->isRedundancyEnabled;
}

void fromApiToResource(const ApiMediaServerUserAttributesData& src, QnMediaServerUserAttributesPtr& dst) {
    dst->serverID = src.serverID;
    dst->name = src.serverName;
    dst->maxCameras = src.maxCameras;
    dst->isRedundancyEnabled = src.allowAutoRedundancy;
}

void fromApiToResourceList(const ApiMediaServerUserAttributesDataList &src, QnMediaServerUserAttributesList& dst) {
    dst.reserve( dst.size()+src.size() );
    for( const ApiMediaServerUserAttributesData& serverAttrs: src )
    {
        QnMediaServerUserAttributesPtr dstElement( new QnMediaServerUserAttributes() );
        fromApiToResource( serverAttrs, dstElement );
        dst.push_back( std::move(dstElement) );
    }
}

void fromResourceListToApi(const QnMediaServerUserAttributesList& src, ApiMediaServerUserAttributesDataList& dst) {
    dst.reserve(dst.size() + src.size());
    for(const QnMediaServerUserAttributesPtr& camerAttrs: src) {
        dst.push_back(ApiMediaServerUserAttributesData());
        fromResourceToApi(camerAttrs, dst.back());
    }
}



////////////////////////////////////////////////////////////
//// ApiResourceData
////////////////////////////////////////////////////////////

void fromResourceToApi(const QnResourcePtr &src, ApiResourceData &dst) {
    Q_ASSERT(!src->getId().isNull());
    Q_ASSERT(!src->getTypeId().isNull());

    dst.id = src->getId();
    dst.typeId = src->getTypeId();
    dst.parentId = src->getParentId();
    dst.name = src->getName();
    dst.url = src->getUrl();
    //dst.status = src->getStatus();
    dst.status = Qn::NotDefined; // status field MUST be modified via setStatus call only

}

void fromApiToResource(const ApiResourceData &src, QnResourcePtr &dst) {
    dst->setId(src.id);
    //dst->setGuid(guid);
    dst->QnResource::setName(src.name); //setting resource name, but not camera name or server name
    dst->setTypeId(src.typeId);
    dst->setParentId(src.parentId);
    dst->setUrl(src.url);
    dst->setStatus(src.status, true);

}

void fromApiToResourceList(const ApiResourceDataList &src, QnResourceList &dst, QnResourceFactory *factory) {
    dst.reserve(dst.size() + (int)src.size());
    for(const ApiResourceData &srcResource: src) {
        dst.push_back(factory->createResource(srcResource.typeId, QnResourceParams(srcResource.id, srcResource.url, QString())));
        fromApiToResource(srcResource, dst.back());
    }
}

void fromResourceListToApi(const ec2::ApiResourceParamDataList &src, ApiResourceParamDataList &dst) {
    dst = src;
}

void fromApiToResourceList(const ApiResourceParamDataList &src, ec2::ApiResourceParamDataList &dst) {
    dst = src;
}

void fromApiToResource(const ApiResourceTypeData &src, QnResourceTypePtr &dst) {
    dst->setId(src.id);
    dst->setName(src.name);
    dst->setManufacture(src.vendor);

    if (!src.parentId.empty())
        dst->setParentId(src.parentId[0]);
    for (size_t i = 1; i < src.parentId.size(); ++i)
        dst->addAdditionalParent(src.parentId[i]);

    for(const ApiPropertyTypeData &p: src.propertyTypes)
        dst->addParamType(p.name, p.defaultValue);
}

void fromApiToResourceList(const ApiResourceTypeDataList &src, QnResourceTypeList &dst) {
    dst.reserve((int)src.size() + dst.size());

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

    dst->setPermissions(src.permissions);
    dst->setDigest(src.digest);
}

void fromResourceToApi(const QnUserResourcePtr &src, ApiUserData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));
    dst.hash = src->getHash();
    dst.digest = src->getDigest();
    dst.isAdmin = src->isAdmin();
    dst.permissions = src->getPermissions();
    dst.email = src->getEmail();
}

template<class List>
void fromApiToResourceList(const ApiUserDataList &src, List &dst, const overload_tag &) {
    dst.reserve(dst.size() + (int)src.size());

    for(const ApiUserData &srcUser: src) {
        QnUserResourcePtr dstUser(new QnUserResource());
        fromApiToResource(srcUser, dstUser);
        dst.push_back(std::move(dstUser));
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
    dst.layout     = src.layoutGuid;
    dst.pcUuid     = src.pcGuid;
    dst.name       = src.name;
    dst.screenSnaps.left() = QnScreenSnap::decode(src.snapLeft);
    dst.screenSnaps.top() = QnScreenSnap::decode(src.snapTop);
    dst.screenSnaps.right() = QnScreenSnap::decode(src.snapRight);
    dst.screenSnaps.bottom() = QnScreenSnap::decode(src.snapBottom);
}

void fromResourceToApi(const QnVideoWallItem &src, ApiVideowallItemData &dst) {
    dst.guid        = src.uuid;
    dst.layoutGuid  = src.layout;
    dst.pcGuid      = src.pcUuid;
    dst.name        = src.name;
    dst.snapLeft    = src.screenSnaps.left().encode();
    dst.snapTop     = src.screenSnaps.top().encode();
    dst.snapRight   = src.screenSnaps.right().encode();
    dst.snapBottom  = src.screenSnaps.bottom().encode();
}

void fromApiToResource(const ApiVideowallMatrixData &src, QnVideoWallMatrix &dst) {
    dst.uuid       = src.id;
    dst.name       = src.name;
    dst.layoutByItem.clear();
    for (const ApiVideowallMatrixItemData &item: src.items)
        dst.layoutByItem[item.itemGuid] = item.layoutGuid;
}

void fromResourceToApi(const QnVideoWallMatrix &src, ApiVideowallMatrixData &dst) {
    dst.id          = src.uuid;
    dst.name        = src.name;
    dst.items.clear();
    dst.items.reserve(src.layoutByItem.size());
    for (auto it = src.layoutByItem.constBegin(); it != src.layoutByItem.constEnd(); ++it) {
        ApiVideowallMatrixItemData item;
        item.itemGuid = it.key();
        item.layoutGuid = it.value();
        dst.items.push_back(item);
    }
}


void fromApiToResource(const ApiVideowallScreenData &src, QnVideoWallPcData::PcScreen &dst) {
    dst.index            = src.pcIndex;
    dst.desktopGeometry  = QRect(src.desktopLeft, src.desktopTop, src.desktopWidth, src.desktopHeight);
    dst.layoutGeometry   = QRect(src.layoutLeft, src.layoutTop, src.layoutWidth, src.layoutHeight);
}

void fromResourceToApi(const QnVideoWallPcData::PcScreen &src, ApiVideowallScreenData &dst) {
    dst.pcIndex         = src.index;
    dst.desktopLeft     = src.desktopGeometry.x();
    dst.desktopTop      = src.desktopGeometry.y();
    dst.desktopWidth    = src.desktopGeometry.width();
    dst.desktopHeight   = src.desktopGeometry.height();
    dst.layoutLeft      = src.layoutGeometry.x();
    dst.layoutTop       = src.layoutGeometry.y();
    dst.layoutWidth     = src.layoutGeometry.width();
    dst.layoutHeight    = src.layoutGeometry.height();
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
    dst->items()->setItems(outItems);

    QnVideoWallPcDataMap pcs;
    for (const ApiVideowallScreenData &screen: src.screens) {
        QnVideoWallPcData::PcScreen outScreen;
        fromApiToResource(screen, outScreen);
        QnVideoWallPcData& outPc = pcs[screen.pcGuid];
        outPc.uuid = screen.pcGuid;
        outPc.screens << outScreen;
    }
    dst->pcs()->setItems(pcs);

    QnVideoWallMatrixList outMatrices;
    for (const ApiVideowallMatrixData &matrixData: src.matrices) {
        outMatrices << QnVideoWallMatrix();
        fromApiToResource(matrixData, outMatrices.last());
    }
    dst->matrices()->setItems(outMatrices);

}

void fromResourceToApi(const QnVideoWallResourcePtr &src, ApiVideowallData &dst) {
    fromResourceToApi(src, static_cast<ApiResourceData &>(dst));

    dst.autorun = src->isAutorun();

    const QnVideoWallItemMap& resourceItems = src->items()->getItems();
    dst.items.clear();
    dst.items.reserve(resourceItems.size());
    for (const QnVideoWallItem &item: resourceItems) {
        ApiVideowallItemData itemData;
        fromResourceToApi(item, itemData);
        dst.items.push_back(itemData);
    }

    dst.screens.clear();
    for (const QnVideoWallPcData &pc: src->pcs()->getItems()) {
        for (const QnVideoWallPcData::PcScreen &screen: pc.screens) {
            ApiVideowallScreenData screenData;
            fromResourceToApi(screen, screenData);
            screenData.pcGuid = pc.uuid;
            dst.screens.push_back(screenData);
        }
    }

    const QnVideoWallMatrixMap &matrices = src->matrices()->getItems();
    dst.matrices.clear();
    dst.matrices.reserve(matrices.size());
    for (const QnVideoWallMatrix &matrix: matrices) {
        ApiVideowallMatrixData matrixData;
        fromResourceToApi(matrix, matrixData);
        dst.matrices.push_back(matrixData);
    }
}

template<class List>
void fromApiToResourceList(const ApiVideowallDataList &src, List &dst, const overload_tag &) {
    dst.reserve(dst.size() + (int)src.size());

    for(const ApiVideowallData &srcVideowall: src) {
        QnVideoWallResourcePtr dstVideowall(new QnVideoWallResource());
        fromApiToResource(srcVideowall, dstVideowall);
        dst.push_back(std::move(dstVideowall));
    }
}

void fromApiToResourceList(const ApiVideowallDataList &src, QnResourceList &dst) {
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const ApiVideowallDataList &src, QnVideoWallResourceList &dst) {
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResource(const ApiVideowallControlMessageData &data, QnVideoWallControlMessage &message) {
    message.operation = static_cast<QnVideoWallControlMessage::Operation>(data.operation);
    message.videoWallGuid = data.videowallGuid;
    message.instanceGuid = data.instanceGuid;
    message.params.clear();
    for (const std::pair<QString, QString> &pair : data.params)
        message.params[pair.first] = pair.second;
}

void fromResourceToApi(const QnVideoWallControlMessage &message, ApiVideowallControlMessageData &data) {
    data.operation = static_cast<int>(message.operation);
    data.videowallGuid = message.videoWallGuid;
    data.instanceGuid = message.instanceGuid;
    data.params.clear();
    auto iter = message.params.constBegin();

    while (iter != message.params.constEnd()) {
        data.params.insert(std::pair<QString, QString>(iter.key(), iter.value()));
        ++iter;
    }
}

void fromApiToResource(const ApiCameraBookmarkTagDataList &data, QnCameraBookmarkTags &tags) {
    for (const ApiCameraBookmarkTagData &tag: data)
        tags << tag.name;
}

void fromResourceToApi(const QnCameraBookmarkTags &tags, ApiCameraBookmarkTagDataList &data) {
    data.reserve(data.size() + tags.size());
    for (const QString &tag: tags)
        data.push_back(ApiCameraBookmarkTagData(tag));
}



} // namespace ec2
