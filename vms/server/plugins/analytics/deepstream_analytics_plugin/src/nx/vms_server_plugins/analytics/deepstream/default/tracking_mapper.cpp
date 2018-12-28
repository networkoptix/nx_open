#include "tracking_mapper.h"

#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>
#define NX_PRINT_PREFIX "deepstream::TrackingMapper::"
#include <nx/kit/debug.h>

#include <plugins/plugin_tools.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

TrackingMapper::TrackingMapper(int objectLifetime):
    m_lifetime(objectLifetime)
{
    NX_OUTPUT << __func__ << " Creating tracking mapper, object lifetime is " << objectLifetime;
}

nxpl::NX_GUID TrackingMapper::getMapping(int nvidiaTrackingId)
{
    NX_OUTPUT
        << __func__
        << " Getting a mapping for object with tracking id " << nvidiaTrackingId;

    auto itr = m_trackingMap.find(nvidiaTrackingId);
    if(itr != m_trackingMap.cend())
    {
        itr->second.found = true;
        return itr->second.guid;
    }

    return kNullGuid;
}

void TrackingMapper::addMapping(int nvidiaTrackingId, const nxpl::NX_GUID& nxObjectId)
{
    NX_OUTPUT
        << __func__
        << " Adding a mapping for object with tracking id "
        << ",  Nx Guid" << nxObjectId;

    m_trackingMap.emplace(nvidiaTrackingId, TrackedObject(nxObjectId, m_lifetime));
}

void TrackingMapper::setLabelMapping(std::map<LabelMappingId, LabelMapping> labelMapping)
{
    m_labelMapping = labelMapping;
}

std::deque<nx::sdk::analytics::Attribute> TrackingMapper::attributes(
    const ROIMeta_Params& roiMeta)
{
    NX_OUTPUT
        << __func__
        << " Getting attributes for an object with tracking id " << roiMeta.tracking_id;

    auto objectItr = m_trackingMap.find(roiMeta.tracking_id);
    if (objectItr == m_trackingMap.cend())
        return std::deque<nx::sdk::analytics::Attribute>();

    if (!roiMeta.has_new_info)
        return objectItr->second.attributes;

    std::deque<nx::sdk::analytics::Attribute> result;
    for (const auto& entry: m_labelMapping)
    {
        const auto mappingId = entry.first;
        const auto& labelMapping = entry.second;
        const auto& labelInfo = roiMeta.label_info[mappingId];

        for (auto i = 0; i < labelInfo.num_labels; ++i)
        {
            const auto labelTypeId = labelInfo.label_ids[i];
            const auto labelTypeMappingItr = labelMapping.find(labelTypeId);
            if (labelTypeMappingItr == labelMapping.cend())
                continue;

            const auto labelValueId = labelInfo.label_outputs[i];
            const auto labelValueStringItr = labelTypeMappingItr->second
                .labelValueMapping
                .find(labelValueId);

            if (labelValueStringItr == labelTypeMappingItr->second.labelValueMapping.cend())
                continue;

            NX_OUTPUT
                << __func__
                << " Got attribute for an object with tracking id " << roiMeta.tracking_id
                << ". Attribute name: " << labelTypeMappingItr->second.labelTypeString
                << ", attribute value: " << labelValueStringItr->second;

            result.emplace_back(
                nx::sdk::IAttribute::Type::string,
                labelTypeMappingItr->second.labelTypeString,
                labelValueStringItr->second);
        }
    }

    objectItr->second.attributes = std::move(result);
    return objectItr->second.attributes;
}

void TrackingMapper::notifyFrameProcessed()
{
    NX_OUTPUT << __func__ << " Frame has been processed, correcting object lifetimes";
    for (auto itr = m_trackingMap.begin(); itr != m_trackingMap.end();)
    {
        auto& trackedObject = itr->second;
        if (!trackedObject.found)
            --trackedObject.lifetime;
        else
            trackedObject.lifetime = m_lifetime;

        if (itr->second.lifetime < 0)
            itr = m_trackingMap.erase(itr);
        else
            ++itr;

        trackedObject.found = false;
    }
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
