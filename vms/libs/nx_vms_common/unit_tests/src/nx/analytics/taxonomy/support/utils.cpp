// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtCore/QFile>

#include <nx/fusion/model_functions.h>

namespace nx::analytics::taxonomy {

bool loadDescriptorsTestData(const QString& filePath, TestData* outTestData)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return false;

    if (!QJson::deserialize(file.readAll(), &outTestData->fullData))
        return false;

    if (!QJson::deserialize(
        outTestData->fullData.contains("descriptors")
            ? outTestData->fullData["descriptors"].toObject()
            : outTestData->fullData,
        &outTestData->descriptors))
    {
        return false;
    }

    return true;
}

} // namespace nx::analytics::taxonomy
