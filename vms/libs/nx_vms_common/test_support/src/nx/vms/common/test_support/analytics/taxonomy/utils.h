// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

struct NX_VMS_COMMON_TEST_SUPPORT_API TestData
{
    nx::vms::api::analytics::Descriptors descriptors;
    QJsonObject fullData;
};

NX_VMS_COMMON_TEST_SUPPORT_API bool loadDescriptorsTestData(
    const QString& filePath, TestData* outTestData);

NX_VMS_COMMON_TEST_SUPPORT_API bool makeDescriptorsTestData(
    std::string_view body, TestData* outTestData);

} // namespace nx::analytics::taxonomy
