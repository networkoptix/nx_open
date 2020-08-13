#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms_server_plugins::analytics::bosch {

QString Bosch::EventType::fullDescription(bool isActive) const
{
    if (!flags.testFlag(nx::vms::api::analytics::EventTypeFlag::stateDependent))
        return description;

    return description.arg(isActive ? positiveState : negativeState);
}

const Bosch::EventType* Bosch::EngineManifest::eventTypeByInternalName(
    const QString& internalName) const
{
    const auto it = eventTypes.find(internalName);
    return (it != eventTypes.end()) ? &*it : nullptr;
}

const Bosch::ObjectType* Bosch::EngineManifest::objectTypeByInternalName(
    const QString& internalName) const
{
    const auto it = objectTypes.find(internalName);
    return (it != objectTypes.end()) ? &*it : nullptr;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    nx::vms_server_plugins::analytics::bosch::Bosch::EventType,
    (json), BoschEventType_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    nx::vms_server_plugins::analytics::bosch::Bosch::ObjectType,
    (json), BoschObjectType_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    nx::vms_server_plugins::analytics::bosch::Bosch::EngineManifest,
    (json), BoschEngineManifest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    nx::vms_server_plugins::analytics::bosch::Bosch::DeviceAgentManifest,
    (json), BoschDeviceAgentManifest_Fields)

} // namespace nx::vms_server_plugins::analytics::bosch
