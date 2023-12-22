// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_model.h"

#include <iterator>
#include <unordered_map>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceGroupSettings, (json), DeviceGroupSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceModelGeneral, (json), DeviceModelGeneral_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceOptions, (json), DeviceOptions_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceScheduleSettings, (json), DeviceScheduleSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceMotionSettings, (json), DeviceMotionSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceModelV1, (json), DeviceModelV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceModelV3, (json), DeviceModelV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceModelForSearch, (json), DeviceModelForSearch_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceTypeModel, (json), DeviceTypeModel_Fields)

namespace {
// TODO: #skolesnik Solve duplication of constant's definitions.
const QString kCompatibleAnalyticsEnginesProperty = "compatibleAnalyticsEngines";
const QString kMediaCapabilities = "mediaCapabilities";
const QString kMediaStreams = "mediaStreams";
const QString kStreamUrls = "streamUrls";
const QString kUserEnabledAnalyticsEnginesProperty = "userEnabledAnalyticsEngines";

const QString kCredentials("credentials");
const QString kDefaultCredentials("defaultCredentials");
const QString kCameraCapabilities("cameraCapabilities");

void extractParametersToFields(DeviceModelV1* m)
{
    if (const auto it = m->parameters.find(kCredentials); it != m->parameters.end())
    {
        if (const auto value = it->second.toString(); !value.isEmpty())
            m->credentials = Credentials::parseColon(value, /*hidePassword*/ false);
        m->parameters.erase(it);
    }

    if (const auto it = m->parameters.find(kDefaultCredentials); it != m->parameters.end())
    {
        if (!m->credentials)
        {
            if (const auto value = it->second.toString(); !value.isEmpty())
                m->credentials = Credentials::parseColon(value, /*hidePassword*/ false);
        }
        m->parameters.erase(it);
    }

    if (const auto it = m->parameters.find(kCameraCapabilities); it != m->parameters.end())
    {
        m->capabilities = static_cast<DeviceCapabilities>(it->second.toInt());
        m->parameters.erase(it);
    }
    else
    {
        m->capabilities = DeviceCapability::noCapabilities;
    }
}

void extractParametersToFields(DeviceModelV3* m)
{
    extractParametersToFields(static_cast<DeviceModelV1*>(m));

    if (const auto it = m->parameters.find(kCompatibleAnalyticsEnginesProperty); it != m->parameters.end())
    {
        QJson::deserialize(it->second, &m->compatibleAnalyticsEngineIds);
        m->parameters.erase(it);
    }

    if (const auto it = m->parameters.find(kMediaCapabilities); it != m->parameters.end())
    {
        QJson::deserialize(it->second, &m->mediaCapabilities);
        m->parameters.erase(it);
    }

    if (const auto it = m->parameters.find(kMediaStreams); it != m->parameters.end())
    {
        QJsonValue jsonValue = it->second.isObject()
            ? std::move(it->second.toObject()["streams"])
            : std::move(it->second);

        if (NX_ASSERT(jsonValue.isArray()))
        {
            QJsonArray mediaStreams = jsonValue.toArray();
            const size_t s = mediaStreams.size();
            for (QJsonValueRef streamReference: mediaStreams)
            {
                // `nx::vms::common::CameraMediaStreamInfo.transports` is stored as
                // `std::vector<QString>`, hence needs to be converted to enum flags.
                if (!NX_ASSERT(streamReference.isObject()))
                    continue;

                QJsonObject stream = streamReference.toObject();
                std::vector<api::StreamTransportType> transports;
                if (!NX_ASSERT(QJson::deserialize(stream["transports"], &transports)))
                    continue;

                api::StreamTransportTypes flags = {};
                for (auto t: transports)
                    flags |= t;

                stream["transports"] = nx::toString(flags);
                streamReference = std::move(stream);
            }

            NX_ASSERT(QJson::deserialize(mediaStreams, &m->mediaStreams));
        }

        m->parameters.erase(it);
    }

    if (const auto it = m->parameters.find(kStreamUrls);
        it != m->parameters.end() /*&& NX_ASSERT(it->second.isObject())*/)
    {
        m->streamUrls = it->second.toObject();
        m->parameters.erase(it);
    }

    if (const auto it = m->parameters.find(kUserEnabledAnalyticsEnginesProperty);
        it != m->parameters.end())
    {
        m->userEnabledAnalyticsEngineIds = std::vector<QnUuid>();
        auto& value = *m->userEnabledAnalyticsEngineIds;
        QJson::deserialize(it->second, &value);
        m->parameters.erase(it);
    }
}

// Fields that marked as `readonly` for apidoc must not be included here, because they are ignored
// by the Schema, hence are not initialized (empty). Having such fields in the `kExtractedOnRequest`
// list will result in creating the corresponding `parameters["field_name"]` with default emtpy
// value.
void moveFieldsToParameters(DeviceModelV1* m)
{
    // This code has been copied as is. It didn't invalidate the `credentials` field.
    if (m->credentials)
        m->parameters[kCredentials] = m->credentials->asString();
}

// Fields that marked as `readonly` for apidoc must not be included here, because they are ignored
// by the Schema, hence are not initialized (empty). Having such fields in the `kExtractedOnRequest`
// list will result in creating the corresponding `parameters["field_name"]` with default emtpy
// value.
void moveFieldsToParameters(DeviceModelV3* m)
{
    moveFieldsToParameters(static_cast<DeviceModelV1*>(m));

    if (m->userEnabledAnalyticsEngineIds)
    {
        QJson::serialize(*m->userEnabledAnalyticsEngineIds,
            &m->parameters[kUserEnabledAnalyticsEnginesProperty]);
    }
}


struct LessById
{
    template <typename L, typename R>
    bool operator()(const L& lhs, const R& rhs) const
    {
        return lhs.getId() < rhs.getId();
    }
} constexpr lessById{};

// This can and should be done by QueryProcessor.
std::unordered_map<QnUuid, std::vector<ResourceParamData>> toResourceMap(
    std::vector<ResourceParamWithRefData> resourcesWithIds)
{
    std::unordered_map<QnUuid, std::vector<ResourceParamData>> result;
    std::sort(resourcesWithIds.begin(), resourcesWithIds.end(), lessById);

    for (auto l = resourcesWithIds.begin(); l != resourcesWithIds.end();)
    {
        auto r = l;
        while (r != resourcesWithIds.end() && r->getId() == l->getId())
            ++r;

        auto& resources = result[l->getId()];
        resources.reserve(std::distance(l, r));
        std::transform(std::make_move_iterator(l),
            std::make_move_iterator(r),
            std::back_inserter(resources),
            [](ResourceParamWithRefData data) -> ResourceParamData { return std::move(data); });

        l = r;
    }

    return result;
}

template<typename Model>
std::vector<Model> fromCameras(std::vector<CameraData> cameras)
{
    std::vector<Model> result;
    result.reserve(cameras.size());
    std::transform(std::make_move_iterator(cameras.begin()),
        std::make_move_iterator(cameras.end()),
        std::back_inserter(result),
        [](CameraData data) -> Model
        {
            return {DeviceModelGeneral::fromCameraData(std::move(data))};
        });

    return result;
}

template <typename Model>
typename Model::DbUpdateTypes toDbTypes(Model model)
{
    std::optional<ResourceStatusData> statusData;
    if (model.status)
        statusData = ResourceStatusData(model.id, *model.status);

    CameraAttributesData attributes;
    attributes.cameraId = model.id;
    attributes.cameraName = model.name;
    attributes.logicalId = model.logicalId;
    attributes.backupQuality = model.backupQuality;
    if (model.group)
        attributes.userDefinedGroupName = model.group->name;

    attributes.controlEnabled = model.options.isControlEnabled;
    attributes.audioEnabled = model.options.isAudioEnabled;
    attributes.disableDualStreaming = model.options.isDualStreamingDisabled;
    attributes.dewarpingParams = model.options.dewarpingParams.toLatin1();
    attributes.preferredServerId = model.options.preferredServerId;
    attributes.failoverPriority = model.options.failoverPriority;
    attributes.backupQuality = model.options.backupQuality;
    attributes.backupContentType = model.options.backupContentType;
    attributes.backupPolicy = model.options.backupPolicy;

    attributes.scheduleEnabled = model.schedule.isEnabled;
    attributes.scheduleTasks = model.schedule.tasks;

    using namespace std::chrono;
    if (model.schedule.minArchivePeriodS)
        attributes.minArchivePeriodS = seconds(*model.schedule.minArchivePeriodS);
    else if (model.schedule.minArchiveDays)
        attributes.minArchivePeriodS = days(*model.schedule.minArchiveDays);

    if (model.schedule.maxArchivePeriodS)
        attributes.maxArchivePeriodS = seconds(*model.schedule.maxArchivePeriodS);
    else if (model.schedule.maxArchiveDays)
        attributes.maxArchivePeriodS = days(*model.schedule.maxArchiveDays);

    attributes.motionType = model.motion.type;
    attributes.motionMask = model.motion.mask.toLatin1();
    attributes.recordBeforeMotionSec = model.motion.recordBeforeS;
    attributes.recordAfterMotionSec = model.motion.recordAfterS;
    attributes.checkResourceExists = CheckResourceExists::no;

    moveFieldsToParameters(&model);

    // Should be explained why.
    // It is erased on request from `parameters`, does it mean that the Core doesn't use it from `parameters`?
    // If so, why is it present in `parameters` on response?
    model.parameters.erase(kCameraCapabilities);

    auto data = std::move(model).toCameraData();
    auto parameters = static_cast<const ResourceWithParameters&>(model).asList(data.id);
    return {std::move(data), std::move(attributes), std::move(statusData), std::move(parameters)};
}

template <typename Model>
std::vector<Model> fromDbTypes(typename Model::DbListTypes all)
{
    const auto binaryFind =
        [](auto& container, const Model& model)
        {
            using result_type = typename std::decay_t<decltype(container)>::value_type*;
            auto found = std::lower_bound(container.begin(), container.end(), model, lessById);
            if (found != container.end())
                return result_type(&*found);

            return result_type(nullptr);
        };

    // This all can be done by the QueryProcessor or the CrudHandler.
    auto statuses = nx::utils::unique_sorted(
        std::move(std::get<std::vector<ResourceStatusData>>(all)), lessById);
    auto attributes = nx::utils::unique_sorted(
        std::move(std::get<std::vector<CameraAttributesData>>(all)), lessById);
    std::unordered_map<QnUuid, std::vector<ResourceParamData>> resources =
        toResourceMap(std::move(std::get<std::vector<ResourceParamWithRefData>>(all)));
    auto devices = fromCameras<Model>(
        nx::utils::unique_sorted(std::move(std::get<std::vector<CameraData>>(all)), lessById));

    for (Model& m: devices)
    {
        if (auto a = binaryFind(attributes, m); a)
        {
            if (!a->cameraName.isEmpty())
                m.name = a->cameraName;

            m.logicalId = a->logicalId;
            m.backupQuality = a->backupQuality;
            m.isLicenseUsed = a->scheduleEnabled;
            if (!a->userDefinedGroupName.isEmpty())
            {
                if (m.group)
                    m.group->name = a->userDefinedGroupName;
                else
                {
                    NX_DEBUG(typeid(Model),
                        "Device %1 has userDefinedGroupName in DB, but model.group is not set",
                        m.id);
                }
            }

            m.options.isControlEnabled = a->controlEnabled;
            m.options.isAudioEnabled = a->audioEnabled;
            m.options.isDualStreamingDisabled = a->disableDualStreaming;
            m.options.dewarpingParams = a->dewarpingParams;
            m.options.preferredServerId = a->preferredServerId;
            m.options.failoverPriority = a->failoverPriority;
            m.options.backupQuality = a->backupQuality;
            m.options.backupContentType = a->backupContentType;
            m.options.backupPolicy = a->backupPolicy;

            m.schedule.isEnabled = a->scheduleEnabled;
            m.schedule.tasks = a->scheduleTasks;

            using namespace std::chrono;
            m.schedule.minArchivePeriodS = duration_cast<seconds>(a->minArchivePeriodS).count();
            m.schedule.minArchiveDays = duration_cast<days>(a->minArchivePeriodS).count();
            m.schedule.maxArchivePeriodS = duration_cast<seconds>(a->maxArchivePeriodS).count();
            m.schedule.maxArchiveDays = duration_cast<days>(a->maxArchivePeriodS).count();

            m.motion.type = a->motionType;
            m.motion.mask = a->motionMask;
            m.motion.recordBeforeS = a->recordBeforeMotionSec;
            m.motion.recordAfterS = a->recordAfterMotionSec;
        }

        if (auto s = binaryFind(statuses, m); s)
            m.status = s->status;
        else
            m.status = ResourceStatus::offline;

        if (auto f = resources.find(m.id); f != resources.cend())
        {
            for (ResourceParamData& r: f->second)
                static_cast<ResourceWithParameters&>(m).setFromParameter(r);

            resources.erase(f);
        }

        extractParametersToFields(&m);
    }

    return devices;
}

} // namespace

DeviceModelGeneral DeviceModelGeneral::fromCameraData(CameraData data)
{
    return {.id = data.id,
        .physicalId = std::move(data.physicalId),
        .url = std::move(data.url),
        .typeId = data.typeId,
        .name = std::move(data.name),
        .mac = data.mac,
        .serverId = data.parentId,
        .isManuallyAdded = data.manuallyAdded,
        .vendor = std::move(data.vendor),
        .model = std::move(data.model)};
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

DeviceModelV1::DbUpdateTypes DeviceModelV1::toDbTypes() &&
{
    return api::toDbTypes(std::move(*this));
}

std::vector<DeviceModelV1> DeviceModelV1::fromDbTypes(DbListTypes data)
{
    return api::fromDbTypes<DeviceModelV1>(std::move(data));
}

DeviceModelV3::DbUpdateTypes DeviceModelV3::toDbTypes() &&
{
    return api::toDbTypes(std::move(*this));
}

std::vector<DeviceModelV3> DeviceModelV3::fromDbTypes(DbListTypes data)
{
    return api::fromDbTypes<DeviceModelV3>(std::move(data));
}

} // namespace nx::vms::api
