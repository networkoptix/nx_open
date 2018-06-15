#include "connection_types.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::PeerType, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, PeerType,
    (nx::vms::api::PeerType::notDefined, "PT_NotDefined")
    (nx::vms::api::PeerType::server, "PT_Server")
    (nx::vms::api::PeerType::desktopClient, "PT_DesktopClient")
    (nx::vms::api::PeerType::videowallClient, "PT_VideowallClient")
    (nx::vms::api::PeerType::oldMobileClient, "PT_OldMobileClient")
    (nx::vms::api::PeerType::mobileClient, "PT_MobileClient")
    (nx::vms::api::PeerType::cloudServer, "PT_CloudServer")
    (nx::vms::api::PeerType::oldServer, "PT_OldSetver"))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::TimeFlag, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, TimeFlag,
    (nx::vms::api::TimeFlag::none, "TF_none")
    (nx::vms::api::TimeFlag::peerIsNotEdgeServer, "TF_peerIsNotEdgeServer")
    (nx::vms::api::TimeFlag::peerHasMonotonicClock, "TF_peerHasMonotonicClock")
    (nx::vms::api::TimeFlag::peerTimeSetByUser, "TF_peerTimeSetByUser")
    (nx::vms::api::TimeFlag::peerTimeSynchronizedWithInternetServer,
        "TF_peerTimeSynchronizedWithInternetServer")
    (nx::vms::api::TimeFlag::peerIsServer, "TF_peerIsServer"))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::TimeFlags, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, TimeFlags,
    (nx::vms::api::TimeFlag::none, "TF_none")
    (nx::vms::api::TimeFlag::peerIsNotEdgeServer, "TF_peerIsNotEdgeServer")
    (nx::vms::api::TimeFlag::peerHasMonotonicClock, "TF_peerHasMonotonicClock")
    (nx::vms::api::TimeFlag::peerTimeSetByUser, "TF_peerTimeSetByUser")
    (nx::vms::api::TimeFlag::peerTimeSynchronizedWithInternetServer,
        "TF_peerTimeSynchronizedWithInternetServer")
    (nx::vms::api::TimeFlag::peerIsServer, "TF_peerIsServer"))
