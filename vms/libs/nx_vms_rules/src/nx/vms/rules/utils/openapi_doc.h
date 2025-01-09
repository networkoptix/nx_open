// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>

#include <nx/vms/rules/field.h>
#include <nx/vms/rules/utils/api_renamer.h>

#include "../rules_fwd.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules::utils {
static constexpr auto kDescriptionProperty = "description";
static constexpr auto kExampleProperty = "example";
static constexpr auto kDefaultProperty = "default";
static constexpr auto kTypeProperty = "type";
static constexpr auto kEnumProperty = "enum";
static constexpr auto kMinProperty = "minimum";
static constexpr auto kMaxProperty = "maximum";
static constexpr auto kPropertyKey = "properties";
static constexpr auto kDocOpenApiSchemePropertyName = "openApiSchema";

/**
 * Adds default, minimum, max values from Field properties, if presented.
 */
NX_VMS_RULES_API QJsonObject addAdditionalDocFromFieldProperties(
    QJsonObject openApiDescriptor, const QVariantMap& fieldProperties);

template<class FieldType>
static QVariantMap addOpenApiToProperties(QVariantMap properties)
{
    auto visibleIt = properties.find("visible");
    if (visibleIt != properties.end() && !visibleIt->toBool())
        return properties; //< Ignoring not visible in UI fields.

    properties[kDocOpenApiSchemePropertyName] =
        addAdditionalDocFromFieldProperties(FieldType::openApiDescriptor(properties), properties);
    return properties;
}

NX_VMS_RULES_API QJsonObject getPropertyOpenApiDescriptor(
    const QMetaType& metatype, bool addDefaultValue = false);

template<class FieldType>
static QJsonObject constructOpenApiDescriptor()
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
        const auto apiFieldName = toApiFieldName(QString(property.name()), property.userType());
        properties[apiFieldName] = getPropertyOpenApiDescriptor(property.metaType());
    }

    result["properties"] = properties;
    return result;
}

/**
 * Safely updates the OpenAPI document value for a field in the properties object.
 * If the properties object does not contain this field, an assertion will be triggered.
 */
NX_VMS_RULES_API void updatePropertyForField(QJsonObject& openApiObjectDescriptor,
    const QString& fieldName,
    const QString& docPropertyName,
    const QString& value);

struct NX_VMS_RULES_API OpenApiDoc
{
    QJsonArray events;
    QJsonArray actions;
    QJsonObject schemas;

    /**
     * Changes the input string to the appropriate schema name format.
     * Input example : "analyticsObject"
     * Result example: "AnalyticsObject"
     */
    static QString schemaName(const QString& id);

    static OpenApiDoc generate(nx::vms::common::SystemContext* context);
};

}  // namespace nx::vms::rules::utils
