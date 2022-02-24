// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_control_message.h"

#include <nx/reflect/string_conversion.h>
#include <nx/utils/range_adapters.h>

std::string QnVideoWallControlMessage::toString() const
{
    QStringList result;
    result << QString::fromStdString(nx::reflect::toString(operation));
    for (const auto& [key, value]: nx::utils::constKeyValueRange(params))
        result << "  [" + key + ": " + value + "]";
    return result.join('\n').toStdString();
}
