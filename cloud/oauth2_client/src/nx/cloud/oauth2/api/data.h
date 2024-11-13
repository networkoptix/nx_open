// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

namespace nx::cloud::oauth2::api {

struct GetSessionResponse
{
    /* True if the session exists and valid */
    bool active = false;

    /* True if the session was verified with a second factor*/
    bool mfaVerified = false;
};

NX_REFLECTION_INSTRUMENT(GetSessionResponse, (active)(mfaVerified))

} // namespace nx::cloud::oauth2::api
