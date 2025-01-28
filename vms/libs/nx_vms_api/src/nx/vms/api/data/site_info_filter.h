// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

struct NX_VMS_API SiteInfoFilter
{
    /**%apidoc[opt]:option Flag to fetch addition data from deployment cloud service. */
    bool fetchPendingServersInfo = false;
};
#define SiteInfoFilter_Fields (fetchPendingServersInfo)
QN_FUSION_DECLARE_FUNCTIONS(SiteInfoFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SiteInfoFilter, SiteInfoFilter_Fields)

} // namespace nx::vms::api
