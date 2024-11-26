// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

#include "resource_data.h"

namespace nx::vms::api {

/**%apidoc Web page information object.
 * %param[unused] typeId
 * %param[proprietary] parentId
 * %param[readonly] id Web page unique id.
 * %param name Web page name.
 *     %example Web page
 * %param url Web page URL.
 *     %example https://example.com
 */
struct NX_VMS_API WebPageData: ResourceData
{
    WebPageData(): ResourceData(kResourceTypeId) {}

    static const QString kResourceTypeName;
    static const nx::Uuid kResourceTypeId;
};
#define WebPageData_Fields ResourceData_Fields
NX_VMS_API_DECLARE_STRUCT_AND_LIST(WebPageData)
NX_REFLECTION_INSTRUMENT(WebPageData, WebPageData_Fields)

} // namespace nx::vms::api
