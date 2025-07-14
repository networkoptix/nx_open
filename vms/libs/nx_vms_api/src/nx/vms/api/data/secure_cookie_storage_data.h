// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

struct CookieName
{
    std::string name;
};
#define CookieName_Fields (name)
QN_FUSION_DECLARE_FUNCTIONS(CookieName, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(CookieName, CookieName_Fields);

struct NX_VMS_API EncodeValue
{
    /**%apidoc
     * %example cookie_name
     */
    std::string name;

    /**%apidoc
     * %example value_to_encode
     */
    std::string value;

    /**%apidoc[opt]
     * Duration in seconds.
     */
    std::chrono::seconds expiresS = kDefaultDuration;

    static constexpr auto kDefaultDuration = std::chrono::years(2);
};
#define EncodeValue_Fields (name)(value)(expiresS)
QN_FUSION_DECLARE_FUNCTIONS(EncodeValue, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(EncodeValue, EncodeValue_Fields);

struct NX_VMS_API DecodedValue
{
    std::string value;
};
#define DecodedValue_Fields (value)
QN_FUSION_DECLARE_FUNCTIONS(DecodedValue, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DecodedValue, DecodedValue_Fields);

} // namespace nx::vms::api
