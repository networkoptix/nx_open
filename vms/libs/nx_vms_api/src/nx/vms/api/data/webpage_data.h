// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_data.h"

namespace nx::vms::api {

struct NX_VMS_API WebPageData: ResourceData
{
    WebPageData(): ResourceData(kResourceTypeId) {}

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;
};
#define WebPageData_Fields ResourceData_Fields
NX_VMS_API_DECLARE_STRUCT_AND_LIST(WebPageData)

} // namespace nx::vms::api

