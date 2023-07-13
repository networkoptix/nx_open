// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"
#include <qjsonobject.h>

#include <QtCore/QFile>

namespace nx::analytics::taxonomy {

bool loadDescriptorsTestData(const QString& filePath, TestData* outTestData)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return false;

    const QByteArray data = file.readAll();

    auto [fullData, jsonResult] = 
        nx::reflect::json::deserialize<QJsonObject>(data.toStdString());

    outTestData->fullData = fullData;

    if (!jsonResult.success)
        return false;

    const QJsonObject maybeDescriptors = outTestData->fullData.contains("descriptors")
            ? outTestData->fullData["descriptors"].toObject()
            : outTestData->fullData;
    
    const QByteArray maybeDescriptorsAsBytes = QJsonDocument(maybeDescriptors).toJson();

    auto [deserializedDescriptors, result] = 
        nx::reflect::json::deserialize<nx::vms::api::analytics::Descriptors>(maybeDescriptorsAsBytes.toStdString());

    outTestData->descriptors = deserializedDescriptors;

    return result.success;
}

} // namespace nx::analytics::taxonomy
