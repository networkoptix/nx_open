// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QMetaObject>
#include <QMetaProperty>

#include <QtCore/QJsonObject>

#include <nx/vms/rules/ini.h>

#include "../rules_fwd.h"

namespace nx::vms::rules::utils {
static constexpr auto kDocOpenApiSchemePropertyName = "openApiSchema";

void NX_VMS_RULES_API createEventRulesOpenApiDoc(nx::vms::rules::Engine* engine);

QJsonObject NX_VMS_RULES_API getPropertyOpenApiDescriptor(const QMetaType& metatype);

template<class FieldType>
QVariantMap addOpenApiToProperties(QVariantMap properties)
{
    if (!ini().generateOpenApiDoc)
        return properties;

    auto visibleIt = properties.find("visible");
    if (visibleIt != properties.end() && !visibleIt->toBool())
        return properties; //< Ignoring not visible in UI fields.

    properties[kDocOpenApiSchemePropertyName] = FieldType::openApiDescriptor();
    return properties;
}

template<class FieldType>
QJsonObject constructOpenApiDescriptor()
{
    QJsonObject result;
    result["type"] = "object";

    const QMetaObject* metaObject = &FieldType::staticMetaObject;
    static const auto propOffset =
        QObject::staticMetaObject.propertyOffset() + QObject::staticMetaObject.propertyCount();

    // Get properties of field and his ancestor, excluding QObject.
    QJsonObject properties;
    for (int i = propOffset; i < metaObject->propertyCount(); ++i)
    {
        QMetaProperty property = metaObject->property(i);
        properties[property.name()] = getPropertyOpenApiDescriptor(property.metaType());
    }

    result["properties"] = properties;
    return result;
}

}  // namespace nx::vms::rules::utils
