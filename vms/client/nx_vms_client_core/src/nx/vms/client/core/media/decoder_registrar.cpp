// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "decoder_registrar.h"

#include <nx/media/decoder_registrar.h>

namespace nx::vms::client::core {

void DecoderRegistrar::registerDecoders(const QMap<int, QSize>& maxFfmpegResolutions)
{
    nx::media::DecoderRegistrar::registerDecoders(maxFfmpegResolutions);
}

} // namespace nx::vms::client::core
