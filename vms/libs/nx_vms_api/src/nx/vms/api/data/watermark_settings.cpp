// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "watermark_settings.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(WatermarkSettings, (json), WatermarkSettings_Fields)

bool WatermarkSettings::operator==(const WatermarkSettings& other) const
{
    return nx::reflect::equals(*this, other);
}

QByteArray WatermarkSettings::toString() const
{
    return QJson::serialized(this);
}

} // namespace nx::vms::api
