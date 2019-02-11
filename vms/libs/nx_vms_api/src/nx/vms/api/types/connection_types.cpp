#include "connection_types.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, PeerType,
    (nx::vms::api::PeerType::notDefined, "PT_NotDefined")
    (nx::vms::api::PeerType::server, "PT_Server")
    (nx::vms::api::PeerType::desktopClient, "PT_DesktopClient")
    (nx::vms::api::PeerType::videowallClient, "PT_VideowallClient")
    (nx::vms::api::PeerType::oldMobileClient, "PT_OldMobileClient")
    (nx::vms::api::PeerType::mobileClient, "PT_MobileClient")
    (nx::vms::api::PeerType::cloudServer, "PT_CloudServer")
    (nx::vms::api::PeerType::oldServer, "PT_OldServer"))
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::PeerType, (numeric)(debug))

#define RuntimeFlagValues \
    (nx::vms::api::RuntimeFlag::masterCloudSync, "MasterCloudSync") \
    (nx::vms::api::RuntimeFlag::noStorages, "NoStorages")

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::RuntimeFlag, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, RuntimeFlag, RuntimeFlagValues)

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::RuntimeFlags, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, RuntimeFlags, RuntimeFlagValues)
