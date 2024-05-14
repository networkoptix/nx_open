// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

struct NX_VMS_API UserSessionSettings
{
    std::optional<std::chrono::seconds> sessionLimitS;
    std::optional<int> sessionsLimit;
    std::optional<int> sessionsLimitPerUser;

    std::optional<std::chrono::seconds> remoteSessionTimeoutS;
    std::optional<std::chrono::seconds> remoteSessionUpdateS;

    std::optional<bool> useSessionLimitForCloud;

    bool operator==(const UserSessionSettings&) const noexcept = default;
};
#define UserSessionSettings_Fields \
    (sessionLimitS) \
    (sessionsLimit) \
    (sessionsLimitPerUser) \
    (remoteSessionTimeoutS) \
    (remoteSessionUpdateS) \
    (useSessionLimitForCloud)
NX_REFLECTION_INSTRUMENT(UserSessionSettings, UserSessionSettings_Fields)
QN_FUSION_DECLARE_FUNCTIONS(UserSessionSettings, (json), NX_VMS_API)

} // namespace nx::vms::api
