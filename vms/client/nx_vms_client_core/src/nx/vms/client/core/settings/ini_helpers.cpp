// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini_helpers.h"

#include <nx/kit/ini_config.h>

namespace nx::vms::client::core {

QVariant getIniValue(const nx::kit::IniConfig& config, const QString& name)
{
    using ParamType = nx::kit::IniConfig::ParamType;

    const void* data = nullptr;
    ParamType type{};

    const bool success = config.getParamTypeAndValue(name.toLatin1().constData(), &type, &data);
    if (!success || !NX_ASSERT(data))
        return {};

    switch (type)
    {
        case ParamType::boolean: return *static_cast<const bool*>(data);
        case ParamType::integer: return *static_cast<const int*>(data);
        case ParamType::float_: return *static_cast<const float*>(data);
        case ParamType::string: return QString::fromUtf8(*static_cast<const char* const*>(data));
    }

    NX_ASSERT(false, "Unknown parameter type: %1", (int) type);
    return {};
}

} // namespace nx::vms::client::core
