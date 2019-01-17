#include "api_conversion_functions.h"

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
#include <utils/email/email.h>
#include <utils/common/ldap.h>

#include <nx/fusion/serialization/json.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/email_settings_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/action_factory.h>

using namespace nx;
using namespace nx::vms::api;

namespace ec2 {

struct overload_tag {};

void fromApiToResource(const EventRuleData& src, vms::event::RulePtr& dst)
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

void fromResourceToApi(const vms::event::RulePtr& src, EventRuleData& dst)
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

void fromApiToResourceList(const EventRuleDataList& src, vms::event::RuleList& dst)
{
    dst.reserve(dst.size() + (int)src.size());
    for (const EventRuleData& srcRule: src)
    {
        dst.push_back(vms::event::RulePtr(new vms::event::Rule()));
        fromApiToResource(srcRule, dst.back());
    }
}

void fromResourceListToApi(const vms::event::RuleList& src, EventRuleDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const vms::event::RulePtr& srcRule: src)
    {
        dst.push_back(EventRuleData());
        fromResourceToApi(srcRule, dst.back());
    }
}

void fromResourceToApi(const vms::event::AbstractActionPtr& src, EventActionData& dst)
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

void fromApiToResource(const EventActionData& src, vms::event::AbstractActionPtr& dst)
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
//// CameraData
////////////////////////////////////////////////////////////

void fromApiToResource(const CameraData& src, QnVirtualCameraResourcePtr& dst)
{
    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());

    // test if the camera is desktop camera
    if (src.typeId == CameraData::kDesktopCameraTypeId)
        dst->addFlags(Qn::desktop_camera);

    if (src.typeId == CameraData::kWearableCameraTypeId)
        dst->addFlags(Qn::wearable_camera);

    dst->setPhysicalId(src.physicalId);
    dst->setMAC(nx::utils::MacAddress(src.mac));
    dst->setManuallyAdded(src.manuallyAdded);
    dst->setModel(src.model);
    dst->setGroupId(src.groupId);
    dst->setDefaultGroupName(src.groupName);
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

void fromResourceToApi(const QnVirtualCameraResourcePtr& src, CameraData& dst)
{
    fromResourceToApi(src, static_cast<ResourceData&>(dst));

    dst.mac = src->getMAC().toString().toLatin1();
    dst.physicalId = src->getPhysicalId();
    dst.manuallyAdded = src->isManuallyAdded();
    dst.model = src->getModel();
    dst.groupId = src->getGroupId();
    dst.groupName = src->getDefaultGroupName();
    dst.statusFlags = src->statusFlags();
    dst.vendor = src->getVendor();
}

void fromResourceListToApi(const QnVirtualCameraResourceList& src, CameraDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const QnVirtualCameraResourcePtr& srcCamera: src)
    {
        dst.push_back(CameraData());
        fromResourceToApi(srcCamera, dst.back());
    }
}

////////////////////////////////////////////////////////////
//// CameraAttributesData
////////////////////////////////////////////////////////////

void fromResourceToApi(const QnScheduleTask& src, ScheduleTaskData& dst)
{
    dst.startTime = src.startTime;
    dst.endTime = src.endTime;
    dst.recordingType = src.recordingType;
    dst.dayOfWeek = src.dayOfWeek;
    dst.streamQuality = src.streamQuality;
    dst.fps = src.fps;
    dst.bitrateKbps = src.bitrateKbps;
}

void fromApiToResource(const ScheduleTaskData& src, QnScheduleTask& dst)
{
    dst.startTime = src.startTime;
    dst.endTime = src.endTime;
    dst.recordingType = src.recordingType;
    dst.dayOfWeek = src.dayOfWeek;
    dst.streamQuality = src.streamQuality;
    dst.fps = src.fps;
    dst.bitrateKbps = src.bitrateKbps;
}

void fromApiToResource(const CameraAttributesData& src, const QnCameraUserAttributesPtr& dst)
{
    dst->cameraId = src.cameraId;
    dst->name = src.cameraName;
    dst->groupName = src.userDefinedGroupName;
    dst->licenseUsed = src.scheduleEnabled;
    dst->motionType = src.motionType;

    QList<QnMotionRegion> regions;
    parseMotionRegionList(regions, src.motionMask);
    dst->motionRegions = regions;

    QnScheduleTaskList tasks;
    tasks.reserve((int)src.scheduleTasks.size());
    for (const auto& srcTask: src.scheduleTasks)
    {
        tasks.push_back(QnScheduleTask());
        fromApiToResource(srcTask, tasks.back());
    }
    dst->scheduleTasks = tasks;

    dst->audioEnabled = src.audioEnabled;

    dst->disableDualStreaming = src.disableDualStreaming;
    dst->cameraControlDisabled = !src.controlEnabled;
    dst->dewarpingParams = QJson::deserialized<QnMediaDewarpingParams>(src.dewarpingParams);
    dst->minDays = src.minArchiveDays;
    dst->maxDays = src.maxArchiveDays;
    dst->preferredServerId = src.preferredServerId;
    dst->failoverPriority = src.failoverPriority;
    dst->backupQualities = src.backupType;
    dst->logicalId = src.logicalId;
    dst->recordBeforeMotionSec = src.recordBeforeMotionSec;
    dst->recordAfterMotionSec = src.recordAfterMotionSec;
}

void fromResourceToApi(const QnCameraUserAttributesPtr& src, CameraAttributesData& dst)
{
    dst.cameraId = src->cameraId;
    dst.cameraName = src->name;
    dst.userDefinedGroupName = src->groupName;
    dst.scheduleEnabled = src->licenseUsed;
    dst.motionType = src->motionType;

    QList<QnMotionRegion> regions;
    dst.motionMask = serializeMotionRegionList(src->motionRegions).toLatin1();

    dst.scheduleTasks.clear();
    for (const QnScheduleTask& srcTask: src->scheduleTasks)
    {
        dst.scheduleTasks.emplace_back();
        fromResourceToApi(srcTask, dst.scheduleTasks.back());
    }

    dst.audioEnabled = src->audioEnabled;
    dst.disableDualStreaming = src->disableDualStreaming;
    dst.controlEnabled = !src->cameraControlDisabled;
    dst.dewarpingParams = QJson::serialized(src->dewarpingParams);
    dst.minArchiveDays = src->minDays;
    dst.maxArchiveDays = src->maxDays;
    dst.preferredServerId = src->preferredServerId;
    dst.failoverPriority = src->failoverPriority;
    dst.backupType = src->backupQualities;
    dst.logicalId = src->logicalId;
    dst.recordBeforeMotionSec = src->recordBeforeMotionSec;
    dst.recordAfterMotionSec = src->recordAfterMotionSec;
}

void fromApiToResourceList(const CameraAttributesDataList& src, QnCameraUserAttributesList& dst)
{
    dst.reserve(dst.size() + static_cast<int>(src.size()));
    for (const auto& cameraAttrs: src)
    {
        QnCameraUserAttributesPtr dstElement(new QnCameraUserAttributes());
        fromApiToResource(cameraAttrs, dstElement);
        dst.push_back(dstElement);
    }
}

void fromResourceListToApi(const QnCameraUserAttributesList& src, CameraAttributesDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const auto& camerAttrs: src)
    {
        dst.push_back({});
        fromResourceToApi(camerAttrs, dst.back());
    }
}

////////////////////////////////////////////////////////////
//// CameraDataEx
////////////////////////////////////////////////////////////

void fromApiToResource(
    const CameraDataEx& src,
    QnVirtualCameraResourcePtr& dst,
    QnCameraUserAttributePool* attributesPool)
{
    fromApiToResource(static_cast<const CameraData&>(src), dst);
    //TODO #ak using QnCameraUserAttributePool here is not good
    QnCameraUserAttributePool::ScopedLock userAttributesLock(attributesPool, dst->getId());
    fromApiToResource(static_cast<const CameraAttributesData&>(src), *userAttributesLock);

    for (const auto& srcParam: src.addParams)
        dst->setProperty(srcParam.name, srcParam.value, QnResource::NO_MARK_DIRTY);
}

void fromResourceToApi(
    const QnVirtualCameraResourcePtr& src,
    CameraDataEx& dst,
    QnCameraUserAttributePool* attributesPool)
{
    fromResourceToApi(src, static_cast<CameraData&>(dst));
    //TODO #ak using QnCameraUserAttributePool here is not good
    QnCameraUserAttributePool::ScopedLock userAttributesLock(attributesPool, src->getId());
    fromResourceToApi(*userAttributesLock, static_cast<CameraAttributesData&>(dst));

    for (const auto& srcParam: src->getRuntimeProperties())
        dst.addParams.push_back(srcParam);
}

void fromResourceListToApi(
    const QnVirtualCameraResourceList& src,
    CameraDataExList& dst,
    QnCameraUserAttributePool* attributesPool)
{
    dst.reserve(dst.size() + src.size());
    for (const QnVirtualCameraResourcePtr& srcCamera: src)
    {
        dst.push_back(CameraDataEx());
        fromResourceToApi(srcCamera, dst.back(), attributesPool);
    }
}

void fromResourceToApi(const QnEmailSettings& src, EmailSettingsData& dst)
{
    dst.host = src.server;
    dst.port = src.port;
    dst.user = src.user;
    dst.from = src.email;
    dst.password = src.password;
    dst.connectionType = src.connectionType;
}

void fromApiToResource(const EmailSettingsData& src, QnEmailSettings& dst)
{
    dst.server = src.host;
    dst.port = src.port;
    dst.user = src.user;
    dst.email = src.from;
    dst.password = src.password;
    dst.connectionType = src.connectionType;
}

void fromApiToResource(const LayoutItemData& src, QnLayoutItemData& dst)
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

void fromResourceToApi(const QnLayoutItemData& src, LayoutItemData& dst)
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

void fromApiToResource(const LayoutData& src, QnLayoutResourcePtr& dst)
{
    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());

    dst->setCellAspectRatio(src.cellAspectRatio);
    dst->setCellSpacing(src.cellSpacing);
    dst->setLocked(src.locked);
    dst->setLogicalId(src.logicalId);
    dst->setFixedSize({src.fixedWidth, src.fixedHeight});

    dst->setBackgroundImageFilename(src.backgroundImageFilename);
    dst->setBackgroundSize(QSize(src.backgroundWidth, src.backgroundHeight));
    dst->setBackgroundOpacity(src.backgroundOpacity);

    QnLayoutItemDataList dstItems;
    for (const LayoutItemData& srcItem: src.items)
    {
        dstItems.push_back(QnLayoutItemData());
        fromApiToResource(srcItem, dstItems.back());
    }
    dst->setItems(dstItems);
}

void fromResourceToApi(const QnLayoutResourcePtr& src, LayoutData& dst)
{
    fromResourceToApi(src, static_cast<ResourceData&>(dst));

    dst.cellAspectRatio = src->cellAspectRatio();
    dst.cellSpacing = src->cellSpacing();
    dst.locked = src->locked();
    dst.logicalId = src->logicalId();

    const auto fixedSize = src->fixedSize();
    dst.fixedWidth = fixedSize.isEmpty() ? 0 : fixedSize.width();
    dst.fixedHeight = fixedSize.isEmpty() ? 0 : fixedSize.height();

    dst.backgroundImageFilename = src->backgroundImageFilename();
    dst.backgroundWidth = src->backgroundSize().width();
    dst.backgroundHeight = src->backgroundSize().height();
    dst.backgroundOpacity = src->backgroundOpacity();

    const QnLayoutItemDataMap& srcItems = src->getItems();
    dst.items.clear();
    dst.items.reserve(srcItems.size());

    for (const QnLayoutItemData& item: srcItems)
    {
        dst.items.push_back(LayoutItemData());
        fromResourceToApi(item, dst.items.back());
    }
}

template<class List>
void fromApiToResourceList(const LayoutDataList& src, List& dst, const overload_tag&)
{
    dst.reserve(dst.size() + (int)src.size());
    for (const LayoutData& srcLayout: src)
    {
        QnLayoutResourcePtr dstLayout(new QnLayoutResource());
        fromApiToResource(srcLayout, dstLayout);
        dst.push_back(dstLayout);
    }
}

void fromApiToResourceList(const LayoutDataList& src, QnResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const LayoutDataList& src, QnLayoutResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromResourceListToApi(const QnLayoutResourceList& src, LayoutDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const QnLayoutResourcePtr& layout: src)
    {
        dst.push_back(LayoutData());
        fromResourceToApi(layout, dst.back());
    }
}

void fromResourceToApi(const QnLicensePtr& src, LicenseData& dst)
{
    dst.key = src->key();
    dst.licenseBlock = src->rawLicense();
}

void fromApiToResource(const LicenseData& src, QnLicensePtr& dst)
{
    dst->loadLicenseBlock(src.licenseBlock);
    if (dst->key().isEmpty())
        dst->setKey(src.key);
}

void fromResourceToApi(const QnLicensePtr& src, DetailedLicenseData& dst)
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

void fromResourceListToApi(const QnLicenseList& src, LicenseDataList& dst)
{
    dst.reserve(dst.size() + src.size());

    for (const QnLicensePtr& srcLicense: src)
    {
        dst.push_back(LicenseData());
        fromResourceToApi(srcLicense, dst.back());
    }
}

void fromApiToResourceList(const LicenseDataList& src, QnLicenseList& dst)
{
    dst.reserve(dst.size() + (int)src.size());
    for (const LicenseData& srcLicense: src)
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

void fromResourceToApi(const QnStorageResourcePtr& src, StorageData& dst)
{
    fromResourceToApi(src, static_cast<ResourceData&>(dst));

    dst.spaceLimit = src->getSpaceLimit();
    dst.usedForWriting = src->isUsedForWriting();
    dst.storageType = src->getStorageType();
    dst.isBackup = src->isBackup();
}

void fromResourceListToApi(const QnStorageResourceList& src, StorageDataList& dst)
{
    for (const QnStorageResourcePtr& storage: src)
    {
        StorageData dstStorage;
        fromResourceToApi(storage, dstStorage);
        dst.push_back(std::move(dstStorage));
    }
}

void fromApiToResource(const StorageData& src, QnStorageResourcePtr& dst)
{
    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());

    dst->setSpaceLimit(src.spaceLimit);
    dst->setUsedForWriting(src.usedForWriting);
    dst->setStorageType(src.storageType);
    dst->setBackup(src.isBackup);
}

void fromResourceToApi(const QnMediaServerResourcePtr& src, MediaServerData& dst)
{
    fromResourceToApi(src, static_cast<ResourceData&>(dst));

    dst.networkAddresses = serializeNetAddrList(src->getNetAddrList());
    dst.flags = src->getServerFlags();
    dst.version = src->getVersion().toString();
    dst.systemInfo = src->getSystemInfo().toString();
    dst.authKey = src->getAuthKey();
}

void fromApiToResource(const MediaServerData& src, QnMediaServerResourcePtr& dst)
{
    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());

    QList<nx::network::SocketAddress> resNetAddrList;
    deserializeNetAddrList(src.networkAddresses, resNetAddrList, QUrl(src.url).port());

    dst->setNetAddrList(resNetAddrList);
    dst->setServerFlags(src.flags);
    dst->setVersion(nx::utils::SoftwareVersion(src.version));
    dst->setSystemInfo(nx::vms::api::SystemInformation(src.systemInfo));
    dst->setAuthKey(src.authKey);
}

template<class List>
void fromApiToResourceList(const MediaServerDataList& src, List& dst, const overload_tag&, QnCommonModule* commonModule)
{
    dst.reserve(dst.size() + (int)src.size());
    for (const MediaServerData& srcServer: src)
    {
        QnMediaServerResourcePtr dstServer(new QnMediaServerResource(commonModule));
        fromApiToResource(srcServer, dstServer);
        dst.push_back(std::move(dstServer));
    }
}

void fromApiToResourceList(const MediaServerDataList& src, QnResourceList& dst, QnCommonModule* commonModule)
{
    fromApiToResourceList(src, dst, overload_tag(), commonModule);
}

void fromApiToResourceList(const MediaServerDataList& src, QnMediaServerResourceList& dst, QnCommonModule* commonModule)
{
    fromApiToResourceList(src, dst, overload_tag(), commonModule);
}

////////////////////////////////////////////////////////////
//// MediaServerUserAttributesData
////////////////////////////////////////////////////////////
void fromResourceToApi(const QnMediaServerUserAttributesPtr& src, MediaServerUserAttributesData& dst)
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

void fromApiToResource(const MediaServerUserAttributesData& src, QnMediaServerUserAttributesPtr& dst)
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

void fromApiToResourceList(const MediaServerUserAttributesDataList& src, QnMediaServerUserAttributesList& dst)
{
    dst.reserve(dst.size() + static_cast<int>(src.size()));
    for (const MediaServerUserAttributesData& serverAttrs: src)
    {
        QnMediaServerUserAttributesPtr dstElement(new QnMediaServerUserAttributes());
        fromApiToResource(serverAttrs, dstElement);
        dst.push_back(std::move(dstElement));
    }
}

void fromResourceListToApi(const QnMediaServerUserAttributesList& src, MediaServerUserAttributesDataList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const QnMediaServerUserAttributesPtr& camerAttrs: src)
    {
        dst.push_back(MediaServerUserAttributesData());
        fromResourceToApi(camerAttrs, dst.back());
    }
}

////////////////////////////////////////////////////////////
//// ResourceData
////////////////////////////////////////////////////////////

void fromResourceToApi(const QnResourcePtr& src, ResourceData& dst)
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

void fromApiToResource(const ResourceData& src, QnResource* dst) {
    dst->setId(src.id);
    //dst->setGuid(guid);
    dst->QnResource::setName(src.name); //setting resource name, but not camera name or server name
    dst->setTypeId(src.typeId);
    dst->setParentId(src.parentId);
    dst->setUrl(src.url);
    //dst->setStatus(src.status, true);
}

void fromApiToResource(const ResourceTypeData& src, QnResourceTypePtr& dst)
{
    dst->setId(src.id);
    dst->setName(src.name);
    dst->setManufacture(src.vendor);

    if (!src.parentId.empty())
        dst->setParentId(src.parentId[0]);
    for (size_t i = 1; i < src.parentId.size(); ++i)
        dst->addAdditionalParent(src.parentId[i]);

    for (const auto& p: src.propertyTypes)
        dst->addParamType(p.name, p.defaultValue);
}

void fromApiToResourceList(const ResourceTypeDataList& src, QnResourceTypeList& dst)
{
    dst.reserve((int)src.size() + dst.size());

    for (const ResourceTypeData& srcType: src)
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

QnUserResourcePtr fromApiToResource(const UserData& src, QnCommonModule* commonModule)
{
    QnUserResourcePtr dst(new QnUserResource(userResourceType(src.isLdap, src.isCloud)));
    if (commonModule)
        dst->setCommonModule(commonModule);
    fromApiToResource(src, dst);
    return dst;
}

void fromApiToResource(const UserData& src, QnUserResourcePtr& dst)
{
    NX_ASSERT(dst->userType() == userResourceType(src.isLdap, src.isCloud), Q_FUNC_INFO, "Unexpected user type");

    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());

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

void fromResourceToApi(const QnUserResourcePtr& src, UserData& dst)
{
    QnUserType userType = src->userType();
    fromResourceToApi(src, static_cast<ResourceData&>(dst));
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
void fromApiToResourceList(const UserDataList& src, List& dst, const overload_tag&)
{
    dst.reserve(dst.size() + (int)src.size());

    for (const UserData& srcUser: src)
        dst.push_back(fromApiToResource(srcUser));
}

void fromApiToResourceList(const UserDataList& src, QnResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const UserDataList& src, QnUserResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResource(const VideowallItemData& src, QnVideoWallItem& dst)
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

void fromResourceToApi(const QnVideoWallItem& src, VideowallItemData& dst)
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

void fromApiToResource(const VideowallMatrixData& src, QnVideoWallMatrix& dst)
{
    dst.uuid       = src.id;
    dst.name       = src.name;
    dst.layoutByItem.clear();
    for (const VideowallMatrixItemData& item: src.items)
        dst.layoutByItem[item.itemGuid] = item.layoutGuid;
}

void fromResourceToApi(const QnVideoWallMatrix& src, VideowallMatrixData& dst)
{
    dst.id          = src.uuid;
    dst.name        = src.name;
    dst.items.clear();
    dst.items.reserve(src.layoutByItem.size());
    for (auto it = src.layoutByItem.constBegin(); it != src.layoutByItem.constEnd(); ++it)
    {
        VideowallMatrixItemData item;
        item.itemGuid = it.key();
        item.layoutGuid = it.value();
        dst.items.push_back(item);
    }
}

void fromApiToResource(const VideowallScreenData& src, QnVideoWallPcData::PcScreen& dst)
{
    dst.index            = src.pcIndex;
    dst.desktopGeometry  = QRect(src.desktopLeft, src.desktopTop, src.desktopWidth, src.desktopHeight);
    dst.layoutGeometry   = QRect(src.layoutLeft, src.layoutTop, src.layoutWidth, src.layoutHeight);
}

void fromResourceToApi(const QnVideoWallPcData::PcScreen& src, VideowallScreenData& dst)
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

void fromApiToResource(const VideowallData& src, QnVideoWallResourcePtr& dst)
{
    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());

    dst->setAutorun(src.autorun);
    dst->setTimelineEnabled(src.timeline);
    QnVideoWallItemList outItems;
    for (const VideowallItemData& item: src.items)
    {
        outItems << QnVideoWallItem();
        fromApiToResource(item, outItems.last());
    }
    dst->items()->setItems(outItems);

    QnVideoWallPcDataMap pcs;
    for (const VideowallScreenData& screen: src.screens)
    {
        QnVideoWallPcData::PcScreen outScreen;
        fromApiToResource(screen, outScreen);
        QnVideoWallPcData& outPc = pcs[screen.pcGuid];
        outPc.uuid = screen.pcGuid;
        outPc.screens << outScreen;
    }
    dst->pcs()->setItems(pcs);

    QnVideoWallMatrixList outMatrices;
    for (const VideowallMatrixData& matrixData: src.matrices)
    {
        outMatrices << QnVideoWallMatrix();
        fromApiToResource(matrixData, outMatrices.last());
    }
    dst->matrices()->setItems(outMatrices);

}

void fromResourceToApi(const QnVideoWallResourcePtr& src, VideowallData& dst)
{
    fromResourceToApi(src, static_cast<ResourceData&>(dst));

    dst.autorun = src->isAutorun();
    dst.timeline = src->isTimelineEnabled();

    const QnVideoWallItemMap& resourceItems = src->items()->getItems();
    dst.items.clear();
    dst.items.reserve(resourceItems.size());
    for (const QnVideoWallItem& item: resourceItems)
    {
        VideowallItemData itemData;
        fromResourceToApi(item, itemData);
        dst.items.push_back(itemData);
    }

    dst.screens.clear();
    for (const QnVideoWallPcData& pc: src->pcs()->getItems())
    {
        for (const QnVideoWallPcData::PcScreen& screen: pc.screens)
        {
            VideowallScreenData screenData;
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
        VideowallMatrixData matrixData;
        fromResourceToApi(matrix, matrixData);
        dst.matrices.push_back(matrixData);
    }
}

template<class List>
void fromApiToResourceList(const VideowallDataList& src, List& dst, const overload_tag&)
{
    dst.reserve(dst.size() + (int)src.size());

    for (const VideowallData& srcVideowall: src)
    {
        QnVideoWallResourcePtr dstVideowall(new QnVideoWallResource());
        fromApiToResource(srcVideowall, dstVideowall);
        dst.push_back(std::move(dstVideowall));
    }
}

void fromApiToResourceList(const VideowallDataList& src, QnResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const VideowallDataList& src, QnVideoWallResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResource(const VideowallControlMessageData& data, QnVideoWallControlMessage& message)
{
    message.operation = static_cast<QnVideoWallControlMessage::Operation>(data.operation);
    message.videoWallGuid = data.videowallGuid;
    message.instanceGuid = data.instanceGuid;
    message.params.clear();
    for (const std::pair<QString, QString>& pair : data.params)
        message.params[pair.first] = pair.second;
}

void fromResourceToApi(const QnVideoWallControlMessage& message, VideowallControlMessageData& data)
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

void fromApiToResource(const WebPageData& src, QnWebPageResourcePtr& dst)
{
    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());
}

void fromResourceToApi(const QnWebPageResourcePtr& src, WebPageData& dst)
{
    fromResourceToApi(src, static_cast<ResourceData&>(dst));
}

template<class List>
void fromApiToResourceList(const WebPageDataList& src, List& dst, const overload_tag&)
{
    dst.reserve(dst.size() + (int)src.size());

    for (const auto& srcPage: src)
    {
        QnWebPageResourcePtr dstPage(new QnWebPageResource());
        fromApiToResource(srcPage, dstPage);
        dst.push_back(std::move(dstPage));
    }
}

void fromApiToResourceList(const WebPageDataList& src, QnResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

void fromApiToResourceList(const WebPageDataList& src, QnWebPageResourceList& dst)
{
    fromApiToResourceList(src, dst, overload_tag());
}

QList<nx::network::SocketAddress> moduleInformationEndpoints(
    const nx::vms::api::ModuleInformationWithAddresses& data)
{
    QList<nx::network::SocketAddress> endpoints;
    for (auto address: data.remoteAddresses)
    {
        //NX_ASSERT(!address.isEmpty());
        if (address.isEmpty())
            continue;
        const bool isIpV6 = address.count(':') > 1;
        if (isIpV6 && !address.startsWith('['))
            address = '[' + address + ']';

        nx::network::SocketAddress endpoint(address);
        if (endpoint.port == 0)
            endpoint.port = (quint16) data.port;

        endpoints << std::move(endpoint);
    }

    return endpoints;
}

void setModuleInformationEndpoints(
    nx::vms::api::ModuleInformationWithAddresses& data,
    const QList<nx::network::SocketAddress>& endpoints)
{
    data.remoteAddresses.clear();
    for (const auto& endpoint : endpoints)
    {
        if (endpoint.port == data.port)
            data.remoteAddresses.insert(endpoint.address.toString());
        else
            data.remoteAddresses.insert(endpoint.toString());
    }
}

} // namespace ec2
