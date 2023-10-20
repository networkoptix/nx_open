// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "api_conversion_functions.h"

#include <api/model/password_data.h>
#include <core/misc/schedule_task.h>
#include <core/misc/screen_snap.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_properties.h>
#include <licensing/license.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/app_info.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std_helpers.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/action_factory.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/rule.h>
#include <utils/email/email.h>

using namespace nx;
using namespace nx::vms::api;

namespace {

template<typename T>
QVector<T> fromStdVector(const std::vector<T>& v)
{
    return {v.begin(), v.end()};
}

} // namespace

namespace ec2 {

struct overload_tag {};

void fromApiToResource(const EventRuleData& src, vms::event::RulePtr& dst)
{
    dst->setId(src.id);
    dst->setEventType(src.eventType);

    dst->setEventResources(fromStdVector(src.eventResourceIds));

    dst->setEventParams(QJson::deserialized<vms::event::EventParameters>(src.eventCondition));

    dst->setEventState(src.eventState);
    dst->setActionType(src.actionType);

    dst->setActionResources(fromStdVector(src.actionResourceIds));

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

    dst.eventResourceIds = nx::toStdVector(src->eventResources());
    dst.actionResourceIds = nx::toStdVector(src->actionResources());

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
    dst.resourceIds = nx::toStdVector(src->getResources());

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

    dst->setResources(fromStdVector(src.resourceIds));

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

    if (src.typeId == CameraData::kVirtualCameraTypeId)
        dst->addFlags(Qn::virtual_camera);

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
    const auto dstUid = dst->getPhysicalId();
    const auto uidToId = QnVirtualCameraResource::physicalIdToId(dstUid);
    if (dstId == uidToId)
        return;

    NX_ASSERT(false, "Malformed camera id: id = %1; physicalId = %2; uniqueIdToId = %3",
        dstId.toString(), dstUid, uidToId.toString());
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
//// CameraDataEx
////////////////////////////////////////////////////////////

void fromApiToResource(const CameraDataEx& src, QnVirtualCameraResourcePtr& dst)
{
    fromApiToResource(static_cast<const CameraData&>(src), dst);
    dst->setUserAttributes(static_cast<const CameraAttributesData&>(src));
    for (const auto& srcParam: src.addParams)
        dst->setProperty(srcParam.name, srcParam.value, /*markDirty*/ false);
}

void fromResourceToApi(const QnVirtualCameraResourcePtr& src, CameraDataEx& dst)
{
    fromResourceToApi(src, static_cast<CameraData&>(dst));
    static_cast<CameraAttributesData&>(dst) = src->getUserAttributes();

    for (const auto& srcParam: src->getProperties())
        dst.addParams.push_back(srcParam);
}

void fromResourceListToApi(
    const QnVirtualCameraResourceList& src,
    CameraDataExList& dst)
{
    dst.reserve(dst.size() + src.size());
    for (const QnVirtualCameraResourcePtr& srcCamera: src)
    {
        dst.push_back(CameraDataEx());
        fromResourceToApi(srcCamera, dst.back());
    }
}

void fromApiToResource(const LayoutItemData& src, nx::vms::common::LayoutItemData& dst)
{
    dst.uuid = src.id;
    dst.flags = src.flags;
    dst.combinedGeometry = QRectF(QPointF(src.left, src.top), QPointF(src.right, src.bottom));
    dst.rotation = src.rotation;
    dst.resource.id =  src.resourceId;
    dst.resource.path = src.resourcePath;
    dst.zoomRect = QRectF(QPointF(src.zoomLeft, src.zoomTop), QPointF(src.zoomRight, src.zoomBottom));
    dst.zoomTargetUuid = src.zoomTargetId;
    dst.contrastParams = src.contrastParams;
    dst.dewarpingParams = src.dewarpingParams;
    dst.displayInfo = src.displayInfo;
    dst.controlPtz = src.controlPtz;
    dst.displayAnalyticsObjects = src.displayAnalyticsObjects;
    dst.displayRoi = src.displayRoi;
    dst.displayHotspots = src.displayHotspots;
}

void fromResourceToApi(const nx::vms::common::LayoutItemData& src, LayoutItemData& dst)
{
    dst.id = src.uuid;
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
    dst.zoomTargetId = src.zoomTargetUuid;
    dst.contrastParams = src.contrastParams;
    dst.dewarpingParams = src.dewarpingParams;
    dst.displayInfo = src.displayInfo;
    dst.controlPtz = src.controlPtz;
    dst.displayAnalyticsObjects = src.displayAnalyticsObjects;
    dst.displayRoi = src.displayRoi;
    dst.displayHotspots = src.displayHotspots;
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

    nx::vms::common::LayoutItemDataList dstItems;
    for (const LayoutItemData& srcItem: src.items)
    {
        dstItems.push_back(nx::vms::common::LayoutItemData());
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

    const nx::vms::common::LayoutItemDataMap& srcItems = src->getItems();
    dst.items.clear();
    dst.items.reserve(srcItems.size());

    for (const nx::vms::common::LayoutItemData& item: srcItems)
    {
        dst.items.push_back(LayoutItemData());
        fromResourceToApi(item, dst.items.back());
    }
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
    dst.orderType = src->orderType();
    dst.company = src->regionalSupport().company;
    dst.support = src->regionalSupport().address;
    dst.deactivations = src->deactivationsCount();
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
    for (const auto& part: source.split(';', Qt::SkipEmptyParts))
    {
        nx::network::SocketAddress endpoint(part.toUtf8());
        if (endpoint.port == 0)
            endpoint.port = (uint16_t) defaultPort;

        target.push_back(std::move(endpoint));
    }
}

static QString serializeNetAddrList(const QList<nx::network::SocketAddress>& netAddrList)
{
    QStringList result;
    for (const auto& addr : netAddrList)
        result << addr.toString().c_str();
    return result.join(';');
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
    dst.osInfo = src->getOsInfo().toString();
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
    dst->setOsInfo(nx::utils::OsInfo::fromString(src.osInfo));
    dst->setAuthKey(src.authKey);
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
    dst->setIdUnsafe(src.id);
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

QnUserResourcePtr fromApiToResource(const UserData& src)
{
    QnUserResourcePtr dst(new QnUserResource(src.type, src.externalId));
    fromApiToResource(src, dst);
    return dst;
}

void fromApiToResource(const UserData& src, const QnUserResourcePtr& dst)
{
    NX_ASSERT(dst->userType() == src.type, "Unexpected user type");

    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());

    dst->setEnabled(src.isEnabled);
    dst->setEmail(src.email);
    dst->setGroupIds(src.groupIds);
    dst->setFullName(src.fullName);
    dst->setRawPermissions(src.permissions);
    dst->setAttributes(src.attributes);
    dst->setExternalId(src.externalId);

    dst->setPasswordHashes({
        QString::fromStdString(nx::network::AppInfo::realm()),
        src.hash,
        src.digest,
        src.cryptSha512Hash});

    dst->setResourceAccessRights(src.resourceAccessRights);
}

void fromResourceToApi(const QnUserResource& src, UserData& dst)
{
    dst.id = src.getId();
    dst.isEnabled = src.isEnabled();
    dst.permissions = src.getRawPermissions();
    if (auto context = src.systemContext())
        dst.fullName = context->resourcePropertyDictionary()->value(dst.id, Qn::USER_FULL_NAME);

    NX_MUTEX_LOCKER locker(&src.m_mutex);
    dst.parentId = src.m_parentId;
    dst.name = src.m_name;
    dst.url = src.m_url;
    dst.email = src.m_email;
    dst.groupIds = src.m_groupIds;
    dst.externalId = src.m_externalId;
    dst.type = src.m_userType;
    dst.attributes = src.m_attributes;
    dst.resourceAccessRights = src.m_resourceAccessRights;
    dst.cryptSha512Hash = src.m_cryptSha512Hash;
    dst.digest = src.m_digest;
    dst.hash = src.m_hash.toString();

    if (dst.fullName.isNull())
        dst.fullName = src.m_fullName;
}

void fromResourceToApi(const QnUserResourcePtr& src, UserData& dst)
{
    if (!NX_ASSERT(src))
        return;
    fromResourceToApi(*src, dst);
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

void fromApiToResource(
    const AnalyticsPluginData& src,
    nx::vms::common::AnalyticsPluginResourcePtr& dst)
{
    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());
}

void fromResourceToApi(
    const nx::vms::common::AnalyticsPluginResourcePtr& src,
    nx::vms::api::AnalyticsPluginData& dst)
{
    fromResourceToApi(src, static_cast<ResourceData&>(dst));
}

void fromApiToResource(
    const AnalyticsEngineData& src,
    nx::vms::common::AnalyticsEngineResourcePtr& dst)
{
    fromApiToResource(static_cast<const ResourceData&>(src), dst.data());
}

void fromResourceToApi(
    const nx::vms::common::AnalyticsEngineResourcePtr& src,
    nx::vms::api::AnalyticsEngineData& dst)
{
    fromResourceToApi(src, static_cast<ResourceData&>(dst));
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

        nx::network::SocketAddress endpoint(address.toUtf8());
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
            data.remoteAddresses.insert(endpoint.address.toString().c_str());
        else
            data.remoteAddresses.insert(endpoint.toString().c_str());
    }
}

} // namespace ec2
