#include "api_conversion_functions.h"

#include <nx/fusion/serialization/json.h>

#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/action_factory.h>

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
#include <core/resource/webpage_resource.h>
#include <core/misc/screen_snap.h>

#include <nx_ec/ec_api.h>

#include "api_business_rule_data.h"
#include "api_camera_data.h"
#include "api_camera_attributes_data.h"
#include "api_camera_data_ex.h"
#include "api_camera_history_data.h"
#include "api_email_data.h"
#include "api_layout_data.h"
#include "api_license_data.h"
#include "api_media_server_data.h"
#include "api_resource_data.h"
#include "api_resource_type_data.h"
#include "api_user_data.h"
#include "api_videowall_data.h"
#include "api_peer_data.h"
#include "api_runtime_data.h"
#include "api_webpage_data.h"

#include <utils/email/email.h>
#include <utils/common/ldap.h>
#include <nx/network/socket_common.h>

#include <nx/utils/log/assert.h>

using namespace nx;

namespace ec2 {

struct overload_tag {};

void fromApiToResource(const ApiBusinessRuleData& src, vms::event::RulePtr& dst)
{
    dst->setId(src.id);
    dst->setEventType(src.eventType);

    dst->setEventResources(QVector<QnUuid>::fromStdVector(src.eventResourceIds));

    dst->setEventParams(QJson::deserialized<vms::event::EventParameters>(src.eventCondition));

    dst->setEventState(src.eventState);
    dst->setActionType(src.actionType);

    dst->setActionResources(QVector<QnUuid>::fromStdVector(src.actionResourceIds));

    dst->setActionParams(QJson::deserialized<vms::event::ActionParameters>(src.actionParams));

    dst->setAggregationPeriod(src.aggregationPeriod);
    dst->setDisabled(src.disabled);
    dst->setComment(src.comment);
    dst->setSchedule(src.schedule);
    dst->setSystem(src.system);
}

void fromResourceToApi(const vms::event::RulePtr& src, ApiBusinessRuleData& dst)
{
    dst.id = src->id();
    dst.eventType = src->eventType();

    dst.eventResourceIds = src->eventResources().toStdVector();
    dst.actionResourceIds = src->actionResources().toStdVector();

    dst.eventCondition = QJson::serialized(src->eventParams());
    dst.actionParams = QJson::serialized(src->actionParams());

    dst.eventState = src->eventState();
    dst.actionType = src->actionType();
    dst.aggregationPeriod = src->aggregationPeriod();
    dst.disabled = src->isDisabled();
    dst.comment = src->comment();
    dst.schedule = src->schedule();
    dst.system = src->isSystem();
}

void fromApiToResourceList(const ApiBusinessRuleDataList& src, vms::event::RuleList& dst)
{
    dst.reserve(dst.size() + (int)src.size());
    for (const ApiBusinessRuleData& srcRule: src)
    {
        dst.push_back(vms::event::RulePtr(new vms::event::Rule()));
        fromApiToResource(srcRule, dst.back());
    }
}

void fromResourceListToApi(const vms::event::RuleList& src, ApiBusinessRuleDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const vms::event::RulePtr& srcRule: src)
    {
        dst.push_back(ApiBusinessRuleData());
        fromResourceToApi(srcRule, dst.back());
    }
}

void fromResourceToApi(const vms::event::AbstractActionPtr& src, ApiBusinessActionData& dst)
{
    dst.actionType = src->actionType();
    dst.toggleState = src->getToggleState();
    dst.receivedFromRemoteHost = src->isReceivedFromRemoteHost();
    dst.resourceIds = src->getResources().toStdVector();

    dst.params = QJson::serialized(src->getParams());
    dst.runtimeParams = QJson::serialized(src->getRuntimeParams());

    dst.ruleId = src->getRuleId();
    dst.aggregationCount = src->getAggregationCount();
}

void fromApiToResource(const ApiBusinessActionData& src, vms::event::AbstractActionPtr& dst)
{
    dst = vms::event::ActionFactory::createAction(src.actionType, QJson::deserialized<vms::event::EventParameters>(src.runtimeParams));

    dst->setToggleState(src.toggleState);
    dst->setReceivedFromRemoteHost(src.receivedFromRemoteHost);

    dst->setResources(QVector<QnUuid>::fromStdVector(src.resourceIds));

    dst->setParams(QJson::deserialized<vms::event::ActionParameters>(src.params));

    dst->setRuleId(src.ruleId);
    dst->setAggregationCount(src.aggregationCount);
}


////////////////////////////////////////////////////////////
//// ApiCameraData
////////////////////////////////////////////////////////////

void fromApiToResource(const ApiCameraData& src, QnVirtualCameraResourcePtr& dst)
{
    fromApiToResource(static_cast<const ApiResourceData&>(src), dst.data());

    // test if the camera is desktop camera
    if (src.typeId == QnResourceTypePool::kDesktopCameraTypeUuid)
        dst->addFlags(Qn::desktop_camera);

    dst->setPhysicalId(src.physicalId);
    dst->setMAC(nx::network::QnMacAddress(src.mac));
    dst->setManuallyAdded(src.manuallyAdded);
    dst->setModel(src.model);
    dst->setGroupId(src.groupId);
    dst->setGroupName(src.groupName);
    dst->setStatusFlags(src.statusFlags);

    dst->setVendor(src.vendor);

    // Validate camera unique id
    const auto dstId = dst->getId();
    const auto dstUid = dst->getUniqueId();
    const auto uidToId = QnVirtualCameraResource::physicalIdToId(dstUid);
    if (dstId == uidToId)
        return;

    QString message = lit("Malformed camera id: id = %1; uniqueId = %2; uniqueIdToId = %3")
        .arg(dstId.toString()).arg(dstUid).arg(uidToId.toString());
    NX_ASSERT(false, "fromApiToResource()", message);
}

void fromResourceToApi(const QnVirtualCameraResourcePtr& src, ApiCameraData& dst)
{
    fromResourceToApi(src, static_cast<ApiResourceData&>(dst));

    dst.mac = src->getMAC().toString().toLatin1();
    dst.physicalId = src->getPhysicalId();
    dst.manuallyAdded = src->isManuallyAdded();
    dst.model = src->getModel();
    dst.groupId = src->getGroupId();
    dst.groupName = src->getDefaultGroupName();
    dst.statusFlags = src->statusFlags();
    dst.vendor = src->getVendor();
}

void fromResourceListToApi(const QnVirtualCameraResourceList& src, ApiCameraDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const QnVirtualCameraResourcePtr& srcCamera: src)
    {
        dst.push_back(ApiCameraData());
        fromResourceToApi(srcCamera, dst.back());
    }
}


////////////////////////////////////////////////////////////
//// ApiCameraAttributesData
////////////////////////////////////////////////////////////

void fromResourceToApi(const QnScheduleTask& src, ApiScheduleTaskData& dst)
{
    dst.startTime = src.getStartTime();
    dst.endTime = src.getEndTime();
    dst.recordAudio = src.getDoRecordAudio();
    dst.recordingType = src.getRecordingType();
    dst.dayOfWeek = src.getDayOfWeek();
    dst.beforeThreshold = src.getBeforeThreshold();
    dst.afterThreshold = src.getAfterThreshold();
    dst.streamQuality = src.getStreamQuality();
    dst.fps = src.getFps();
    dst.bitrateKbps = src.getBitrateKbps();
}

void fromApiToResource(const ApiScheduleTaskData& src, QnScheduleTask& dst, const QnUuid& resourceId)
{
    dst = QnScheduleTask(
        resourceId, 
        src.dayOfWeek, 
        src.startTime, 
        src.endTime, 
        src.recordingType,
        src.beforeThreshold, 
        src.afterThreshold, 
        src.streamQuality, 
        src.fps, 
        src.recordAudio, 
        src.bitrateKbps);
}

void fromApiToResource(const ApiCameraAttributesData& src, const QnCameraUserAttributesPtr& dst)
{
    dst->cameraId = src.cameraId;
    dst->name = src.cameraName;
    dst->groupName = src.userDefinedGroupName;
    dst->scheduleDisabled = !src.scheduleEnabled;
    dst->licenseUsed = src.licenseUsed;
    dst->motionType = src.motionType;

    QList<QnMotionRegion> regions;
    parseMotionRegionList(regions, src.motionMask);
    dst->motionRegions = regions;

    QnScheduleTaskList tasks;
    tasks.reserve((int)src.scheduleTasks.size());
    for (const ApiScheduleTaskData& srcTask: src.scheduleTasks)
    {
        tasks.push_back(QnScheduleTask());
        fromApiToResource(srcTask, tasks.back(), src.cameraId);
    }
    dst->scheduleTasks = tasks;

    dst->audioEnabled = src.audioEnabled;

    dst->secondaryQuality = src.secondaryStreamQuality;
    dst->cameraControlDisabled = !src.controlEnabled;
    dst->dewarpingParams = QJson::deserialized<QnMediaDewarpingParams>(src.dewarpingParams);
    dst->minDays = src.minArchiveDays;
    dst->maxDays = src.maxArchiveDays;
    dst->preferredServerId = src.preferredServerId;
    dst->failoverPriority = src.failoverPriority;
    dst->backupQualities = src.backupType;
}

void fromResourceToApi(const QnCameraUserAttributesPtr& src, ApiCameraAttributesData& dst)
{
    dst.cameraId = src->cameraId;
    dst.cameraName = src->name;
    dst.userDefinedGroupName = src->groupName;
    dst.scheduleEnabled = !src->scheduleDisabled;
    dst.licenseUsed = src->licenseUsed;
    dst.motionType = src->motionType;

    QList<QnMotionRegion> regions;
    dst.motionMask = serializeMotionRegionList(src->motionRegions).toLatin1();

    dst.scheduleTasks.clear();
    for (const QnScheduleTask& srcTask: src->scheduleTasks)
    {
        dst.scheduleTasks.push_back(ApiScheduleTaskData());
        fromResourceToApi(srcTask, dst.scheduleTasks.back());
    }

    dst.audioEnabled = src->audioEnabled;
    dst.secondaryStreamQuality = src->secondaryQuality;
    dst.controlEnabled = !src->cameraControlDisabled;
    dst.dewarpingParams = QJson::serialized(src->dewarpingParams);
    dst.minArchiveDays = src->minDays;
    dst.maxArchiveDays = src->maxDays;
    dst.preferredServerId = src->preferredServerId;
    dst.failoverPriority = src->failoverPriority;
    dst.backupType = src->backupQualities;
}

void fromApiToResourceList(const ApiCameraAttributesDataList& src, QnCameraUserAttributesList& dst)
{
    dst.reserve(dst.size() + static_cast<int>(src.size()));
    for (const ApiCameraAttributesData& cameraAttrs: src)
    {
        QnCameraUserAttributesPtr dstElement(new QnCameraUserAttributes());
        fromApiToResource(cameraAttrs, dstElement);
        dst.push_back(std::move(dstElement));
    }
}

void fromResourceListToApi(const QnCameraUserAttributesList& src, ApiCameraAttributesDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const QnCameraUserAttributesPtr& camerAttrs: src)
    {
        dst.push_back(ApiCameraAttributesData());
        fromResourceToApi(camerAttrs, dst.back());
    }
}


////////////////////////////////////////////////////////////
//// ApiCameraDataEx
////////////////////////////////////////////////////////////

void fromApiToResource(
    const ApiCameraDataEx& src,
    QnVirtualCameraResourcePtr& dst,
    QnCameraUserAttributePool* attributesPool)
{
    fromApiToResource(static_cast<const ApiCameraData&>(src), dst);
    //TODO #ak using QnCameraUserAttributePool here is not good
    QnCameraUserAttributePool::ScopedLock userAttributesLock(attributesPool, dst->getId());
    fromApiToResource(static_cast<const ApiCameraAttributesData&>(src), *userAttributesLock);

    for (const ApiResourceParamData& srcParam: src.addParams)
        dst->setProperty(srcParam.name, srcParam.value, QnResource::NO_MARK_DIRTY);
}

void fromResourceToApi(
    const QnVirtualCameraResourcePtr& src,
    ApiCameraDataEx& dst,
    QnCameraUserAttributePool* attributesPool)
{
    fromResourceToApi(src, static_cast<ApiCameraData&>(dst));
    //TODO #ak using QnCameraUserAttributePool here is not good
    QnCameraUserAttributePool::ScopedLock userAttributesLock(attributesPool, src->getId());
    fromResourceToApi(*userAttributesLock, static_cast<ApiCameraAttributesData&>(dst));

    for (const ec2::ApiResourceParamData& srcParam: src->getRuntimeProperties())
        dst.addParams.push_back(srcParam);
}

void fromResourceListToApi(
    const QnVirtualCameraResourceList& src,
    ApiCameraDataExList& dst,
    QnCameraUserAttributePool* attributesPool)
{
    dst.reserve(dst.size() + src.size());
    for (const QnVirtualCameraResourcePtr& srcCamera: src)
    {
        dst.push_back(ApiCameraDataEx());
        fromResourceToApi(srcCamera, dst.back(), attributesPool);
    }
}

void fromResourceToApi(const QnEmailSettings& src, ApiEmailSettingsData& dst)
{
    dst.host = src.server;
    dst.port = src.port;
    dst.user = src.user;
    dst.from = src.email;
    dst.password = src.password;
    dst.connectionType = src.connectionType;
}

void fromApiToResource(const ApiEmailSettingsData& src, QnEmailSettings& dst)
{
    dst.server = src.host;
    dst.port = src.port;
    dst.user = src.user;
    dst.email = src.from;
    dst.password = src.password;
    dst.connectionType = src.connectionType;
}

void fromApiToResource(const ApiLayoutItemData& src, QnLayoutItemData& dst)
{
    dst.uuid = src.id;
    dst.flags = src.flags;
    dst.combinedGeometry = QRectF(QPointF(src.left, src.top), QPointF(src.right, src.bottom));
    dst.rotation = src.rotation;
    dst.resource.id =  src.resourceId;
    dst.resource.uniqueId = src.resourcePath;
    dst.zoomRect = QRectF(QPointF(src.zoomLeft, src.zoomTop), QPointF(src.zoomRight, src.zoomBottom));
    dst.zoomTargetUuid = src.zoomTargetId;
    dst.contrastParams = ImageCorrectionParams::deserialize(src.contrastParams);
    dst.dewarpingParams = QJson::deserialized<QnItemDewarpingParams>(src.dewarpingParams);
    dst.displayInfo = src.displayInfo;
}

void fromResourceToApi(const QnLayoutItemData& src, ApiLayoutItemData& dst)
{
    dst.id = src.uuid;
    dst.flags = src.flags;
    dst.left = src.combinedGeometry.topLeft().x();
    dst.top = src.combinedGeometry.topLeft().y();
    dst.right = src.combinedGeometry.bottomRight().x();
    dst.bottom = src.combinedGeometry.bottomRight().y();
    dst.rotation = src.rotation;
    dst.resourceId = src.resource.id;
    dst.resourcePath = src.resource.uniqueId;
    dst.zoomLeft = src.zoomRect.topLeft().x();
    dst.zoomTop = src.zoomRect.topLeft().y();
    dst.zoomRight = src.zoomRect.bottomRight().x();
    dst.zoomBottom = src.zoomRect.bottomRight().y();
    dst.zoomTargetId = src.zoomTargetUuid;
    dst.contrastParams = src.contrastParams.serialize();
    dst.dewarpingParams = QJson::serialized(src.dewarpingParams);
    dst.displayInfo = src.displayInfo;
}

void fromApiToResource(const ApiLayoutData& src, QnLayoutResourcePtr& dst)
{
    fromApiToResource(static_cast<const ApiResourceData&>(src), dst.data());

    dst->setCellAspectRatio(src.cellAspectRatio);
    dst->setCellSpacing(src.horizontalSpacing);
    dst->setLocked(src.locked);
    dst->setBackgroundImageFilename(src.backgroundImageFilename);
    dst->setBackgroundSize(QSize(src.backgroundWidth, src.backgroundHeight));
    dst->setBackgroundOpacity(src.backgroundOpacity);

    QnLayoutItemDataList dstItems;
    for (const ApiLayoutItemData& srcItem: src.items)
    {
        dstItems.push_back(QnLayoutItemData());
        fromApiToResource(srcItem, dstItems.back());
    }
    dst->setItems(dstItems);
}

void fromResourceToApi(const QnLayoutResourcePtr& src, ApiLayoutData& dst)
{
    fromResourceToApi(src, static_cast<ApiResourceData&>(dst));

    dst.cellAspectRatio = src->cellAspectRatio();
    dst.horizontalSpacing = src->cellSpacing();
    dst.verticalSpacing = src->cellSpacing(); // TODO: #ynikitenkov Remove vertical spacing?
    dst.locked = src->locked();
    dst.backgroundImageFilename = src->backgroundImageFilename();
    dst.backgroundWidth = src->backgroundSize().width();
    dst.backgroundHeight = src->backgroundSize().height();
    dst.backgroundOpacity = src->backgroundOpacity();

    const QnLayoutItemDataMap& srcItems = src->getItems();
    dst.items.clear();
    dst.items.reserve(srcItems.size());

    for (const QnLayoutItemData& item: srcItems)
    {
        dst.items.push_back(ApiLayoutItemData());
        fromResourceToApi(item, dst.items.back());
    }
}

template<class List>
void fromApiToResourceList(const ApiLayoutDataList& src, List& dst, const overload_tag&)
{
    dst.reserve(dst.size() + (int)src.size());
    for (const ApiLayoutData& srcLayout: src)
    {
        QnLayoutResourcePtr dstLayout(new QnLayoutResource());
        fromApiToResource(srcLayout, dstLayout);
        dst.push_back(dstLayout);
    }
}

void fromApiToResourceList(const ApiLayoutDataList& src, QnResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const ApiLayoutDataList& src, QnLayoutResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromResourceListToApi(const QnLayoutResourceList& src, ApiLayoutDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const QnLayoutResourcePtr& layout: src)
    {
        dst.push_back(ApiLayoutData());
        fromResourceToApi(layout, dst.back());
    }
}

void fromResourceToApi(const QnLicensePtr& src, ApiLicenseData& dst)
{
    dst.key = src->key();
    dst.licenseBlock = src->rawLicense();
}

void fromApiToResource(const ApiLicenseData& src, QnLicensePtr& dst)
{
    dst->loadLicenseBlock(src.licenseBlock);
    if (dst->key().isEmpty())
        dst->setKey(src.key);
}

void fromResourceToApi(const QnLicensePtr& src, ApiDetailedLicenseData& dst)
{
    dst.key = src->key();
    dst.signature = src->signature();
    dst.name = src->name();
    dst.cameraCount = src->cameraCount();
    dst.hardwareId = src->hardwareId();
    dst.licenseType = src->xclass();
    dst.version = src->version();
    dst.brand = src->brand();
    dst.expiration = src->expiration();
}

void fromResourceListToApi(const QnLicenseList& src, ApiLicenseDataList& dst)
{
    dst.reserve(dst.size() + src.size());

    for (const QnLicensePtr& srcLicense: src)
    {
        dst.push_back(ApiLicenseData());
        fromResourceToApi(srcLicense, dst.back());
    }
}

void fromApiToResourceList(const ApiLicenseDataList& src, QnLicenseList& dst)
{
    dst.reserve(dst.size() + (int)src.size());
    for (const ApiLicenseData& srcLicense: src)
    {
        dst.push_back(QnLicensePtr(new QnLicense()));
        fromApiToResource(srcLicense, dst.back());
    }
}

void deserializeNetAddrList(const QString& source, QList<nx::network::SocketAddress>& target, int defaultPort)
{
    for (const auto& part: source.split(L';', QString::SkipEmptyParts))
    {
        nx::network::SocketAddress endpoint(part);
        if (endpoint.port == 0)
            endpoint.port = (uint16_t) defaultPort;

        target.push_back(std::move(endpoint));
    }
}

static QString serializeNetAddrList(const QList<nx::network::SocketAddress>& netAddrList)
{
    QStringList result;
    for (const auto& addr : netAddrList)
        result << addr.toString();
    return result.join(L';');
}

void fromResourceToApi(const QnStorageResourcePtr& src, ApiStorageData& dst)
{
    fromResourceToApi(src, static_cast<ApiResourceData&>(dst));

    dst.spaceLimit = src->getSpaceLimit();
    dst.usedForWriting = src->isUsedForWriting();
    dst.storageType = src->getStorageType();
    dst.isBackup = src->isBackup();
}

void fromResourceListToApi(const QnStorageResourceList& src, ApiStorageDataList& dst)
{
    for (const QnStorageResourcePtr& storage: src)
    {
        ApiStorageData dstStorage;
        fromResourceToApi(storage, dstStorage);
        dst.push_back(std::move(dstStorage));
    }
}

void fromApiToResource(const ApiStorageData& src, QnStorageResourcePtr& dst)
{
    fromApiToResource(static_cast<const ApiResourceData&>(src), dst.data());

    dst->setSpaceLimit(src.spaceLimit);
    dst->setUsedForWriting(src.usedForWriting);
    dst->setStorageType(src.storageType);
    dst->setBackup(src.isBackup);
}

void fromResourceToApi(const QnMediaServerResourcePtr& src, ApiMediaServerData& dst)
{
    fromResourceToApi(src, static_cast<ApiResourceData&>(dst));

    dst.networkAddresses = serializeNetAddrList(src->getNetAddrList());
    dst.flags = src->getServerFlags();
    dst.version = src->getVersion().toString();
    dst.systemInfo = src->getSystemInfo().toString();
    dst.authKey = src->getAuthKey();
}

void fromApiToResource(const ApiMediaServerData& src, QnMediaServerResourcePtr& dst)
{
    fromApiToResource(static_cast<const ApiResourceData&>(src), dst.data());

    QList<nx::network::SocketAddress> resNetAddrList;
    deserializeNetAddrList(src.networkAddresses, resNetAddrList, QUrl(src.url).port());

    dst->setNetAddrList(resNetAddrList);
    dst->setServerFlags(src.flags);
    dst->setVersion(QnSoftwareVersion(src.version));
    dst->setSystemInfo(QnSystemInformation(src.systemInfo));
    dst->setAuthKey(src.authKey);
}

template<class List>
void fromApiToResourceList(const ApiMediaServerDataList& src, List& dst, const overload_tag&, QnCommonModule* commonModule)
{
    dst.reserve(dst.size() + (int)src.size());
    for (const ApiMediaServerData& srcServer: src)
    {
        QnMediaServerResourcePtr dstServer(new QnMediaServerResource(commonModule));
        fromApiToResource(srcServer, dstServer);
        dst.push_back(std::move(dstServer));
    }
}

void fromApiToResourceList(const ApiMediaServerDataList& src, QnResourceList& dst, QnCommonModule* commonModule)
{
    fromApiToResourceList(src, dst, overload_tag(), commonModule);
}

void fromApiToResourceList(const ApiMediaServerDataList& src, QnMediaServerResourceList& dst, QnCommonModule* commonModule)
{
    fromApiToResourceList(src, dst, overload_tag(), commonModule);
}


////////////////////////////////////////////////////////////
//// ApiMediaServerUserAttributesData
////////////////////////////////////////////////////////////
void fromResourceToApi(const QnMediaServerUserAttributesPtr& src, ApiMediaServerUserAttributesData& dst)
{
    dst.serverId = src->serverId;
    dst.serverName = src->name;
    dst.maxCameras = src->maxCameras;
    dst.allowAutoRedundancy = src->isRedundancyEnabled;
    dst.backupType = src->backupSchedule.backupType;
    dst.backupDaysOfTheWeek = src->backupSchedule.backupDaysOfTheWeek;
    dst.backupStart = src->backupSchedule.backupStartSec;
    dst.backupDuration = src->backupSchedule.backupDurationSec;
    dst.backupBitrate = src->backupSchedule.backupBitrate;
}

void fromApiToResource(const ApiMediaServerUserAttributesData& src, QnMediaServerUserAttributesPtr& dst)
{
    dst->serverId = src.serverId;
    dst->name = src.serverName;
    dst->maxCameras = src.maxCameras;
    dst->isRedundancyEnabled = src.allowAutoRedundancy;
    dst->backupSchedule.backupType = src.backupType;
    dst->backupSchedule.backupDaysOfTheWeek = src.backupDaysOfTheWeek;
    dst->backupSchedule.backupStartSec = src.backupStart;
    dst->backupSchedule.backupDurationSec = src.backupDuration;
    dst->backupSchedule.backupBitrate = src.backupBitrate;
}

void fromApiToResourceList(const ApiMediaServerUserAttributesDataList& src, QnMediaServerUserAttributesList& dst)
{
    dst.reserve(dst.size() + static_cast<int>(src.size()));
    for (const ApiMediaServerUserAttributesData& serverAttrs: src)
    {
        QnMediaServerUserAttributesPtr dstElement(new QnMediaServerUserAttributes());
        fromApiToResource(serverAttrs, dstElement);
        dst.push_back(std::move(dstElement));
    }
}

void fromResourceListToApi(const QnMediaServerUserAttributesList& src, ApiMediaServerUserAttributesDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const QnMediaServerUserAttributesPtr& camerAttrs: src)
    {
        dst.push_back(ApiMediaServerUserAttributesData());
        fromResourceToApi(camerAttrs, dst.back());
    }
}



////////////////////////////////////////////////////////////
//// ApiResourceData
////////////////////////////////////////////////////////////

void fromResourceToApi(const QnResourcePtr& src, ApiResourceData& dst)
{
    //NX_ASSERT(!src->getId().isNull());
    NX_ASSERT(!src->getTypeId().isNull());

    dst.id = src->getId();
    dst.typeId = src->getTypeId();
    dst.parentId = src->getParentId();
    dst.name = src->getName();
    dst.url = src->getUrl();
    //dst.status = src->getStatus();
    //dst.status = Qn::NotDefined; // status field MUST be modified via setStatus call only
}


void fromApiToResource(const ApiResourceData& src, QnResource* dst) {
    dst->setId(src.id);
    //dst->setGuid(guid);
    dst->QnResource::setName(src.name); //setting resource name, but not camera name or server name
    dst->setTypeId(src.typeId);
    dst->setParentId(src.parentId);
    dst->setUrl(src.url);
    //dst->setStatus(src.status, true);
}

void fromApiToResource(const ApiResourceTypeData& src, QnResourceTypePtr& dst)
{
    dst->setId(src.id);
    dst->setName(src.name);
    dst->setManufacture(src.vendor);

    if (!src.parentId.empty())
        dst->setParentId(src.parentId[0]);
    for (size_t i = 1; i < src.parentId.size(); ++i)
        dst->addAdditionalParent(src.parentId[i]);

    for (const ApiPropertyTypeData& p: src.propertyTypes)
        dst->addParamType(p.name, p.defaultValue);
}

void fromApiToResourceList(const ApiResourceTypeDataList& src, QnResourceTypeList& dst)
{
    dst.reserve((int)src.size() + dst.size());

    for (const ApiResourceTypeData& srcType: src)
    {
        dst.push_back(QnResourceTypePtr(new QnResourceType()));
        fromApiToResource(srcType, dst.back());
    }
}

static QnUserType userResourceType(bool isLdap, bool isCloud)
{
    return isLdap  ? QnUserType::Ldap :
           isCloud ? QnUserType::Cloud:
                     QnUserType::Local;
}

QnUserResourcePtr fromApiToResource(const ApiUserData& src, QnCommonModule* commonModule)
{
    QnUserResourcePtr dst(new QnUserResource(userResourceType(src.isLdap, src.isCloud)));
    if (commonModule)
        dst->setCommonModule(commonModule);
    fromApiToResource(src, dst);
    return dst;
}

void fromApiToResource(const ApiUserData& src, QnUserResourcePtr& dst)
{
    NX_ASSERT(dst->userType() == userResourceType(src.isLdap, src.isCloud), Q_FUNC_INFO, "Unexpected user type");

    fromApiToResource(static_cast<const ApiResourceData&>(src), dst.data());

    dst->setOwner(src.isAdmin);
	dst->setEnabled(src.isEnabled);
    dst->setEmail(src.email);
    dst->setHash(src.hash);
    dst->setUserRoleId(src.userRoleId);
    dst->setFullName(src.fullName);

    dst->setRawPermissions(src.permissions);
    dst->setDigest(src.digest);
    dst->setCryptSha512Hash(src.cryptSha512Hash);
    dst->setRealm(src.realm);
}

void fromResourceToApi(const QnUserResourcePtr& src, ApiUserData& dst)
{
    QnUserType userType = src->userType();
    fromResourceToApi(src, static_cast<ApiResourceData&>(dst));
    dst.hash = src->getHash();
    dst.digest = src->getDigest();
    dst.isAdmin = src->isOwner();
    dst.isLdap = userType == QnUserType::Ldap;
	dst.isEnabled = src->isEnabled();
    dst.permissions = src->getRawPermissions();
    dst.email = src->getEmail();
    dst.cryptSha512Hash = src->getCryptSha512Hash();
    dst.realm = src->getRealm();
    dst.isCloud = userType == QnUserType::Cloud;
    dst.userRoleId = src->userRoleId();
    dst.fullName = src->fullName();
}

template<class List>
void fromApiToResourceList(const ApiUserDataList& src, List& dst, const overload_tag&)
{
    dst.reserve(dst.size() + (int)src.size());

    for (const ApiUserData& srcUser: src)
        dst.push_back(fromApiToResource(srcUser));
}

void fromApiToResourceList(const ApiUserDataList& src, QnResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const ApiUserDataList& src, QnUserResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResource(const ApiVideowallItemData& src, QnVideoWallItem& dst)
{
    dst.uuid       = src.guid;
    dst.layout     = src.layoutGuid;
    dst.pcUuid     = src.pcGuid;
    dst.name       = src.name;
    dst.screenSnaps.left() = QnScreenSnap::decode(src.snapLeft);
    dst.screenSnaps.top() = QnScreenSnap::decode(src.snapTop);
    dst.screenSnaps.right() = QnScreenSnap::decode(src.snapRight);
    dst.screenSnaps.bottom() = QnScreenSnap::decode(src.snapBottom);
}

void fromResourceToApi(const QnVideoWallItem& src, ApiVideowallItemData& dst)
{
    dst.guid        = src.uuid;
    dst.layoutGuid  = src.layout;
    dst.pcGuid      = src.pcUuid;
    dst.name        = src.name;
    dst.snapLeft    = src.screenSnaps.left().encode();
    dst.snapTop     = src.screenSnaps.top().encode();
    dst.snapRight   = src.screenSnaps.right().encode();
    dst.snapBottom  = src.screenSnaps.bottom().encode();
}

void fromApiToResource(const ApiVideowallMatrixData& src, QnVideoWallMatrix& dst)
{
    dst.uuid       = src.id;
    dst.name       = src.name;
    dst.layoutByItem.clear();
    for (const ApiVideowallMatrixItemData& item: src.items)
        dst.layoutByItem[item.itemGuid] = item.layoutGuid;
}

void fromResourceToApi(const QnVideoWallMatrix& src, ApiVideowallMatrixData& dst)
{
    dst.id          = src.uuid;
    dst.name        = src.name;
    dst.items.clear();
    dst.items.reserve(src.layoutByItem.size());
    for (auto it = src.layoutByItem.constBegin(); it != src.layoutByItem.constEnd(); ++it)
    {
        ApiVideowallMatrixItemData item;
        item.itemGuid = it.key();
        item.layoutGuid = it.value();
        dst.items.push_back(item);
    }
}


void fromApiToResource(const ApiVideowallScreenData& src, QnVideoWallPcData::PcScreen& dst)
{
    dst.index            = src.pcIndex;
    dst.desktopGeometry  = QRect(src.desktopLeft, src.desktopTop, src.desktopWidth, src.desktopHeight);
    dst.layoutGeometry   = QRect(src.layoutLeft, src.layoutTop, src.layoutWidth, src.layoutHeight);
}

void fromResourceToApi(const QnVideoWallPcData::PcScreen& src, ApiVideowallScreenData& dst)
{
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

void fromApiToResource(const ApiVideowallData& src, QnVideoWallResourcePtr& dst)
{
    fromApiToResource(static_cast<const ApiResourceData&>(src), dst.data());

    dst->setAutorun(src.autorun);
    QnVideoWallItemList outItems;
    for (const ApiVideowallItemData& item: src.items)
    {
        outItems << QnVideoWallItem();
        fromApiToResource(item, outItems.last());
    }
    dst->items()->setItems(outItems);

    QnVideoWallPcDataMap pcs;
    for (const ApiVideowallScreenData& screen: src.screens)
    {
        QnVideoWallPcData::PcScreen outScreen;
        fromApiToResource(screen, outScreen);
        QnVideoWallPcData& outPc = pcs[screen.pcGuid];
        outPc.uuid = screen.pcGuid;
        outPc.screens << outScreen;
    }
    dst->pcs()->setItems(pcs);

    QnVideoWallMatrixList outMatrices;
    for (const ApiVideowallMatrixData& matrixData: src.matrices)
    {
        outMatrices << QnVideoWallMatrix();
        fromApiToResource(matrixData, outMatrices.last());
    }
    dst->matrices()->setItems(outMatrices);

}

void fromResourceToApi(const QnVideoWallResourcePtr& src, ApiVideowallData& dst)
{
    fromResourceToApi(src, static_cast<ApiResourceData&>(dst));

    dst.autorun = src->isAutorun();

    const QnVideoWallItemMap& resourceItems = src->items()->getItems();
    dst.items.clear();
    dst.items.reserve(resourceItems.size());
    for (const QnVideoWallItem& item: resourceItems)
    {
        ApiVideowallItemData itemData;
        fromResourceToApi(item, itemData);
        dst.items.push_back(itemData);
    }

    dst.screens.clear();
    for (const QnVideoWallPcData& pc: src->pcs()->getItems())
    {
        for (const QnVideoWallPcData::PcScreen& screen: pc.screens)
        {
            ApiVideowallScreenData screenData;
            fromResourceToApi(screen, screenData);
            screenData.pcGuid = pc.uuid;
            dst.screens.push_back(screenData);
        }
    }

    const QnVideoWallMatrixMap& matrices = src->matrices()->getItems();
    dst.matrices.clear();
    dst.matrices.reserve(matrices.size());
    for (const QnVideoWallMatrix& matrix: matrices)
    {
        ApiVideowallMatrixData matrixData;
        fromResourceToApi(matrix, matrixData);
        dst.matrices.push_back(matrixData);
    }
}

template<class List>
void fromApiToResourceList(const ApiVideowallDataList& src, List& dst, const overload_tag&)
{
    dst.reserve(dst.size() + (int)src.size());

    for (const ApiVideowallData& srcVideowall: src)
    {
        QnVideoWallResourcePtr dstVideowall(new QnVideoWallResource());
        fromApiToResource(srcVideowall, dstVideowall);
        dst.push_back(std::move(dstVideowall));
    }
}

void fromApiToResourceList(const ApiVideowallDataList& src, QnResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const ApiVideowallDataList& src, QnVideoWallResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResource(const ApiVideowallControlMessageData& data, QnVideoWallControlMessage& message)
{
    message.operation = static_cast<QnVideoWallControlMessage::Operation>(data.operation);
    message.videoWallGuid = data.videowallGuid;
    message.instanceGuid = data.instanceGuid;
    message.params.clear();
    for (const std::pair<QString, QString>& pair : data.params)
        message.params[pair.first] = pair.second;
}

void fromResourceToApi(const QnVideoWallControlMessage& message, ApiVideowallControlMessageData& data)
{
    data.operation = static_cast<int>(message.operation);
    data.videowallGuid = message.videoWallGuid;
    data.instanceGuid = message.instanceGuid;
    data.params.clear();
    auto iter = message.params.constBegin();

    while (iter != message.params.constEnd())
    {
        data.params.insert(std::pair<QString, QString>(iter.key(), iter.value()));
        ++iter;
    }
}

void fromApiToResource(const ApiWebPageData& src, QnWebPageResourcePtr& dst)
{
    fromApiToResource(static_cast<const ApiResourceData&>(src), dst.data());
}

void fromResourceToApi(const QnWebPageResourcePtr& src, ApiWebPageData& dst)
{
    fromResourceToApi(src, static_cast<ApiResourceData&>(dst));
}

template<class List>
void fromApiToResourceList(const ApiWebPageDataList& src, List& dst, const overload_tag&)
{
    dst.reserve(dst.size() + (int)src.size());

    for (const ApiWebPageData& srcPage: src)
    {
        QnWebPageResourcePtr dstPage(new QnWebPageResource());
        fromApiToResource(srcPage, dstPage);
        dst.push_back(std::move(dstPage));
    }
}

void fromApiToResourceList(const ApiWebPageDataList& src, QnResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const ApiWebPageDataList& src, QnWebPageResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}


} // namespace ec2
