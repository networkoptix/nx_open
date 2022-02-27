// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

struct TestData
{
    nx::vms::api::analytics::Descriptors descriptors;
    QJsonObject fullData;
};

bool loadDescriptorsTestData(const QString& filePath, TestData* outTestData);

} // namespace nx::analytics::taxonomy
