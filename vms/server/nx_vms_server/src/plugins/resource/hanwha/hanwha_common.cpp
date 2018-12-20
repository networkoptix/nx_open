#include "hanwha_common.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::plugins, HanwhaDeviceType,
    (nx::vms::server::plugins::HanwhaDeviceType::unknown, "Unknown")
    (nx::vms::server::plugins::HanwhaDeviceType::nwc, "NWC")
    (nx::vms::server::plugins::HanwhaDeviceType::nvr, "NVR")
    (nx::vms::server::plugins::HanwhaDeviceType::dvr, "DVR")
    (nx::vms::server::plugins::HanwhaDeviceType::encoder, "Encoder")
    (nx::vms::server::plugins::HanwhaDeviceType::decoder, "Decoder")
    (nx::vms::server::plugins::HanwhaDeviceType::hybrid, "Hybrid")
);
