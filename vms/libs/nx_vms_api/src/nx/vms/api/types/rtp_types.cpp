#include "rtp_types.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::RtpTransportType, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, RtpTransportType,
    (nx::vms::api::RtpTransportType::automatic, "Auto")
    (nx::vms::api::RtpTransportType::tcp, "TCP")
    (nx::vms::api::RtpTransportType::udp, "UDP")
    (nx::vms::api::RtpTransportType::udpMulticast, "UDPMulticast"))
