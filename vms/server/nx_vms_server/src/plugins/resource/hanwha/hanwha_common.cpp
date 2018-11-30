#include "hanwha_common.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver_core::plugins, HanwhaDeviceType,
    (nx::mediaserver_core::plugins::HanwhaDeviceType::unknown, "Unknown")
    (nx::mediaserver_core::plugins::HanwhaDeviceType::nwc, "NWC")
    (nx::mediaserver_core::plugins::HanwhaDeviceType::nvr, "NVR")
    (nx::mediaserver_core::plugins::HanwhaDeviceType::dvr, "DVR")
    (nx::mediaserver_core::plugins::HanwhaDeviceType::encoder, "Encoder")
    (nx::mediaserver_core::plugins::HanwhaDeviceType::decoder, "Decoder")
    (nx::mediaserver_core::plugins::HanwhaDeviceType::hybrid, "Hybrid")
);
