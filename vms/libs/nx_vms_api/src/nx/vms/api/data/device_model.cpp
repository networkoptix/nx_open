// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_model.h"

#include <nx/utils/std/algorithm.h>
#include <nx/fusion/model_functions.h>

static const QString kCredentials("credentials");
static const QString kDefaultCredentials("defaultCredentials");
static const QString kCameraCapabilities("cameraCapabilities");

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceGroupSettings, (json), DeviceGroupSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceModelGeneral, (json), DeviceModelGeneral_Fields)

DeviceModelGeneral::DeviceModelGeneral(CameraData data):
    id(std::move(data.id)),
    physicalId(std::move(data.physicalId)),
    url(std::move(data.url)),
    typeId(std::move(data.typeId)),
    name(std::move(data.name)),
    mac(std::move(data.mac)),
    serverId(std::move(data.parentId)),
    isManuallyAdded(std::move(data.manuallyAdded)),
    vendor(std::move(data.vendor)),
    model(std::move(data.model))
{
    if (!data.groupId.isEmpty() || !data.groupName.isEmpty())
        group = DeviceGroupSettings{std::move(data.groupId), std::move(data.groupName)};
}

CameraData DeviceModelGeneral::toCameraData() &&
{
    CameraData data;
    data.id = std::move(id);
    data.physicalId = std::move(physicalId);
    data.name = std::move(name);
    data.url = std::move(url);
    data.typeId = std::move(typeId);
    data.mac = mac.toLatin1();
    data.parentId = std::move(serverId);
    data.manuallyAdded = isManuallyAdded;
    data.vendor = std::move(vendor);
    data.model = std::move(model);
    if (group)
    {
        data.groupId = std::move(group->id);
        data.groupName = std::move(group->name);
    }
    return data;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceOptions, (json), DeviceOptions_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceScheduleSettings, (json), DeviceScheduleSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceMotionSettings, (json), DeviceMotionSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceModel, (json), DeviceModel_Fields)

DeviceModel::DeviceModel(CameraData cameraData):
    DeviceModelGeneral{std::move(cameraData)}
{
}

DeviceModel::DbUpdateTypes DeviceModel::toDbTypes() &&
{
    std::optional<ResourceStatusData> statusData;
    if (status)
        statusData = ResourceStatusData(id, *status);

    CameraAttributesData attributes;
    attributes.cameraId = id;
    attributes.cameraName = name;
    attributes.logicalId = logicalId;
    attributes.backupQuality = backupQuality;
    attributes.licenseUsed = isLicenseUsed;
    if (group)
        attributes.userDefinedGroupName = group->name;

    attributes.controlEnabled = options.isControlEnabled;
    attributes.audioEnabled = options.isAudioEnabled;
    attributes.disableDualStreaming = options.isDualStreamingDisabled;
    attributes.dewarpingParams = options.dewarpingParams.toLatin1();
    attributes.preferredServerId = options.preferredServerId;
    attributes.failoverPriority = options.failoverPriority;
    attributes.backupQuality = options.backupQuality;
    attributes.backupContentType = options.backupContentType;
    attributes.backupPolicy = options.backupPolicy;

    attributes.scheduleEnabled = schedule.isEnabled;
    attributes.scheduleTasks = schedule.tasks;
    if (schedule.minArchiveDays)
        attributes.minArchiveDays = *schedule.minArchiveDays;
    if (schedule.maxArchiveDays)
        attributes.maxArchiveDays = *schedule.maxArchiveDays;

    attributes.motionType = motion.type;
    attributes.motionMask = motion.mask.toLatin1();;
    attributes.recordBeforeMotionSec = motion.recordBeforeS;
    attributes.recordAfterMotionSec = motion.recordAfterS;

    attributes.checkResourceExists = CheckResourceExists::no;

    if (credentials)
        parameters[kCredentials] = credentials->asString();

    parameters.erase(kCameraCapabilities);

    auto data = std::move(*this).toCameraData();
    auto parameters = asList(data.id);
    return {std::move(data), std::move(attributes), std::move(statusData), std::move(parameters)};
}

std::vector<DeviceModel> DeviceModel::fromDbTypes(DbListTypes all)
{
    auto& allAttributes = std::get<CameraAttributesDataList>(all);
    auto& allStatuses = std::get<ResourceStatusDataList>(all);
    return ResourceWithParameters::fillListFromDbTypes<DeviceModel, CameraData>(
        &all,
        [&allAttributes, &allStatuses](CameraData data)
        {
            DeviceModel model(std::move(data));

            auto attributes = nx::utils::find_if(
                allAttributes,
                [&id = model.id](const auto& attributes) { return attributes.cameraId == id; });
            if (attributes)
            {
                if (!attributes->cameraName.isEmpty())
                    model.name = attributes->cameraName;
                model.logicalId = attributes->logicalId;
                model.backupQuality = attributes->backupQuality;
                model.isLicenseUsed = attributes->licenseUsed;
                if (!attributes->userDefinedGroupName.isEmpty())
                {
                    if (model.group)
                    {
                        model.group->name = attributes->userDefinedGroupName;
                    }
                    else
                    {
                        NX_DEBUG(typeid(DeviceModel),
                            "Device %1 has userDefinedGroupName in DB, but model.group is not set",
                            model.id);
                    }
                }

                model.options.isControlEnabled = attributes->controlEnabled;
                model.options.isAudioEnabled = attributes->audioEnabled;
                model.options.isDualStreamingDisabled = attributes->disableDualStreaming;
                model.options.dewarpingParams = attributes->dewarpingParams;
                model.options.preferredServerId = attributes->preferredServerId;
                model.options.failoverPriority = attributes->failoverPriority;
                model.options.backupQuality = attributes->backupQuality;
                model.options.backupContentType = attributes->backupContentType;
                model.options.backupPolicy = attributes->backupPolicy;

                model.schedule.isEnabled = attributes->scheduleEnabled;
                model.schedule.tasks = attributes->scheduleTasks;
                if (attributes->minArchiveDays > 0)
                    model.schedule.minArchiveDays = attributes->minArchiveDays;
                if (attributes->maxArchiveDays > 0)
                    model.schedule.maxArchiveDays = attributes->maxArchiveDays;

                model.motion.type = attributes->motionType;
                model.motion.mask = attributes->motionMask;
                model.motion.recordBeforeS = attributes->recordBeforeMotionSec;
                model.motion.recordAfterS = attributes->recordAfterMotionSec;
            }

            const auto status = nx::utils::find_if(
                allStatuses,
                [id = model.getId()](const auto& status) { return status.id == id; });
            if (status)
                model.status = status->status;

            return model;
        });
}

void DeviceModel::extractFromList(const QnUuid& id, ResourceParamWithRefDataList* list)
{
    ResourceWithParameters::extractFromList(id, list);

    if (const auto it = parameters.find(kCredentials); it != parameters.end())
    {
        if (const auto value = it->second.toString(); !value.isEmpty())
            credentials = Credentials::parseColon(value);
        parameters.erase(it);
    }
    if (const auto it = parameters.find(kDefaultCredentials); it != parameters.end())
    {
        if (!credentials)
        {
            if (const auto value = it->second.toString(); !value.isEmpty())
                credentials = Credentials::parseColon(value);
        }
        parameters.erase(it);
    }

    if (const auto it = parameters.find(kCameraCapabilities); it != parameters.end())
    {
        capabilities = static_cast<DeviceCapabilities>(it->second.toInt());
        parameters.erase(it);
    }
}

} // namespace nx::vms::api
