#include "network_block_data.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::NetworkPortWithPoweringMode, PoweringMode,
    (nx::vms::api::NetworkPortWithPoweringMode::PoweringMode::off, "off")
    (nx::vms::api::NetworkPortWithPoweringMode::PoweringMode::on, "on")
    (nx::vms::api::NetworkPortWithPoweringMode::PoweringMode::automatic, "automatic")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::NetworkPortState, PoweringStatus,
    (nx::vms::api::NetworkPortState::PoweringStatus::disconnected, "disconnected")
    (nx::vms::api::NetworkPortState::PoweringStatus::connected, "connected")
    (nx::vms::api::NetworkPortState::PoweringStatus::powered, "powered")
)

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkPortWithPoweringMode, (json),
    nx_vms_api_NetworkPortWithPoweringMode_Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkPortState, (json), nx_vms_api_NetworkPortState_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkBlockData, (json), nx_vms_api_NetworkBlockData_Fields);

} // namespace nx::vms::api
