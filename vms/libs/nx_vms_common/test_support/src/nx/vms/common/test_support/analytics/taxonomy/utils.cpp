// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/reflect/json.h>

namespace nx::analytics::taxonomy {

bool loadDescriptorsTestData(const QString& filePath, TestData* outTestData)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return false;

    const auto data = file.readAll().toStdString();

    auto [fullData, jsonResult] = nx::reflect::json::deserialize<QJsonObject>(data);

    outTestData->fullData = fullData;

    if (!jsonResult.success)
        return false;

    const QJsonObject maybeDescriptors = outTestData->fullData.contains("descriptors")
        ? outTestData->fullData["descriptors"].toObject()
        : outTestData->fullData;

    const auto maybeDescriptorsAsBytes = QJsonDocument(maybeDescriptors).toJson().toStdString();
    auto [deserializedDescriptors, result] =
        nx::reflect::json::deserialize<nx::vms::api::analytics::Descriptors>(
            maybeDescriptorsAsBytes);

    outTestData->descriptors = deserializedDescriptors;

    return result.success;
}

} // namespace nx::analytics::taxonomy
