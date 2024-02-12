// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::vms::api {

struct HardwareIdMapping
{
    QString hardwareId;
    QString physicalId;
    nx::Uuid physicalIdGuid;
};

#define HardwareIdMapping_Fields (hardwareId)(physicalId)(physicalIdGuid)
NX_VMS_API_DECLARE_STRUCT(HardwareIdMapping)

using HardwareIdMappingList = std::vector<HardwareIdMapping>;

} // namespace nx::vms::api
