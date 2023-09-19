// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/reflect/json.h>
#include <nx/utils/serialization/qt_core_types.h>

namespace nx::vms::client::desktop::jsapi::detail {

/** Helper function which converts data structure to the json object. */
template<typename Data>
QJsonObject toJsonObject(const Data& data)
{
    const auto buffer = nx::reflect::json::serialize(data);
    return QJsonDocument::fromJson(buffer.c_str()).object();
}

/** Helper function which converts json object to the data structure. */
template<typename JsonData, typename Data>
bool fromJsonObject(const JsonData& jsonData, Data& result)
{
    const auto& stringRepresentation = QJsonDocument(jsonData).toJson().toStdString();
    return nx::reflect::json::deserialize(stringRepresentation, &result);
}

/** Helper function which converts data structure to the json array. */
template<typename Data>
QJsonArray toJsonArray(const Data& data)
{
    const auto buffer = nx::reflect::json::serialize(data);
    return QJsonDocument::fromJson(buffer.c_str()).array();
}

} // namespace nx::vms::client::desktop::jsapi::detail
