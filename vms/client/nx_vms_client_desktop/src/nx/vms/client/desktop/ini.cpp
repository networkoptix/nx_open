// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

Ini& ini()
{
    static Ini ini;
    return ini;
}

QVariant Ini::get(const QString& name) const
{
    const void* data = nullptr;
    ParamType type{};

    const bool success = getParamTypeAndValue(name.toLatin1().constData(), &type, &data);
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

bool Ini::isAutoCloudHostDeductionMode() const
{
    return strcmp(cloudHost, "auto") == 0;
}

} // namespace nx::vms::client::desktop
