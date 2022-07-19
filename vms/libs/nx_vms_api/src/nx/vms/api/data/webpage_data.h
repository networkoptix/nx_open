// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

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
    static const QnUuid kResourceTypeId;
};
#define WebPageData_Fields ResourceData_Fields
NX_VMS_API_DECLARE_STRUCT_AND_LIST(WebPageData)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::WebPageData)
Q_DECLARE_METATYPE(nx::vms::api::WebPageDataList)
