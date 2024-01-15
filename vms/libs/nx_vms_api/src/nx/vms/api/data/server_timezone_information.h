// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QString>

#include <nx/reflect/instrument.h>

namespace nx::vms::api {

struct NX_VMS_API ServerTimeZoneInformation
{
    /**%apidoc Time zone offset, in milliseconds. */
    std::chrono::milliseconds timeZoneOffsetMs{0};

    /**%apidoc Identification of the time zone in the text form. */
    QString timeZoneId;
};
#define ServerTimeZoneInformation_Fields (timeZoneOffsetMs)(timeZoneId)
NX_REFLECTION_INSTRUMENT(ServerTimeZoneInformation, ServerTimeZoneInformation_Fields);

} // namespace nx::vms::api
