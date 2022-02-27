// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "flags.h"

#include <QtCore/QString>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std_string_utils.h>

namespace nx::utils::flags_detail {

void assertInvalidFlagValue(const std::type_info& type, int value, int unrecognizedFlags)
{
    NX_ASSERT(false, "toString(%3): Invalid flag value: 0x%1, unrecognized flags: 0x%2",
        QString::number(value, 16), QString::number(unrecognizedFlags, 16), type);
}

void logInvalidFlagValue(const std::type_info& type, int value, int unrecognizedFlags)
{
    NX_DEBUG(type, "toString(): Invalid flag value: 0x%1, unrecognized flags: 0x%2",
        QString::number(value, 16), QString::number(unrecognizedFlags, 16));
}

void logInvalidFlagRepresentation(const std::type_info& type, const std::string_view& flag)
{
    NX_DEBUG(type, "fromString(): Invalid flag representation: %1", flag);
}

std::string_view trim(const std::string_view& str)
{
    return nx::utils::trim(str);
}

} // namespace nx::utils::flags_detail
