// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "types.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

#include <nx/reflect/json.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/serialization/qt_core_types.h>

#include "detail/globals_structures.h"
#include "detail/tab_structures.h"

namespace nx::vms::client::desktop::jsapi {

template<typename Type>
void registerType()
{
    if (QMetaType::hasRegisteredConverterFunction<Type, QVariantMap>())
        return;

    // QWebChannel requires QVariantMap and QVariantList conversions.
    QMetaType::registerConverter<Type, QVariantMap>(
        [](const Type& value)
        {
            const std::string string = nx::reflect::json::serialize(value);
            return QJsonDocument::fromJson(string.c_str()).object().toVariantMap();
        });

    QMetaType::registerConverter<QVariantMap, Type>(
        [](const QVariantMap& value)
        {
            const auto& string =
                QJsonDocument(QJsonObject::fromVariantMap(value)).toJson().toStdString();

            auto [result, _] = nx::reflect::json::deserialize<Type>(string);
            return result;
        });

    QMetaType::registerConverter<QList<Type>, QVariantList>(
        [](const QList<Type>& list)
        {
            QVariantList result;
            for (const auto& item: list)
                result.push_back(QVariant::fromValue(item)); //< QVariantMap conversion.

            return result;
        });

    QMetaType::registerConverter<QVariantList, QList<Type>>(
        [](const QVariantList& list)
        {
            QList<Type> result;
            for (const auto& item: list)
                result.push_back(item.value<Type>()); //< QVariantMap conversion.

            return result;
        });
}

void registerTypes()
{
    registerType<Error>();
    registerType<State>();
    registerType<Item>();
    registerType<ItemResult>();
    registerType<ItemParams>();
    registerType<LayoutProperties>();
    registerType<Resource>();
    registerType<ResourceResult>();
    registerType<ParameterResult>();
}

} // namespace nx::vms::client::desktop::jsapi
