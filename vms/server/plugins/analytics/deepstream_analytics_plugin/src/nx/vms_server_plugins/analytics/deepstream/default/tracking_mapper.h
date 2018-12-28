#pragma once

#include <map>
#include <vector>
#include <deque>

extern "C" {

#include <gstnvivameta_api.h>

} // extern "C"

#include <plugins/plugin_api.h>
#include <nx/sdk/analytics/helpers/attribute.h>
#include <nx/vms_server_plugins/analytics/deepstream/utils.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

using LabelTypeIndex = int;
using LabelValueIndex = int;
using LabelMappingId = int;

struct LabelTypeMapping
{
    std::string labelTypeString;
    std::map<LabelValueIndex, std::string> labelValueMapping;
};

using LabelMapping = std::map<LabelTypeIndex, LabelTypeMapping>;

struct TrackedObject
{
    TrackedObject():
        guid(kNullGuid),
        lifetime(0),
        found(false)
    {
    }

    TrackedObject(const nxpl::NX_GUID& guid, int lifetime = 1):
        guid(guid),
        lifetime(lifetime),
        found(true)
    {
    }

    nxpl::NX_GUID guid;
    int lifetime;
    bool found;
    std::deque<nx::sdk::analytics::Attribute> attributes;
};

class TrackingMapper
{

public:
    TrackingMapper(int objectLifetime);
    nxpl::NX_GUID getMapping(int nvidiaTrackingId);
    void addMapping(int nvidiaTrackingId, const nxpl::NX_GUID& nxObjectId);

    void setLabelMapping(std::map<LabelMappingId, LabelMapping> labelMapping);
    std::deque<nx::sdk::analytics::Attribute> attributes(const ROIMeta_Params& roiMeta);

    void notifyFrameProcessed();

private:
    int m_lifetime;
    std::map<uint, TrackedObject> m_trackingMap;
    std::map<LabelMappingId, LabelMapping> m_labelMapping;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
