// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QMetaObject>
#include <QMetaProperty>
#include <mutex>

#include <QtCore/QJsonObject>

#include <nx/network/rest/handler_pool.h>
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
const int kMinimumVersionRequiresMerging = 4;

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

class NX_VMS_RULES_API VmsRulesOpenApiDocHelper
{
public:
    static VmsRulesOpenApiDocHelper* instance();

    VmsRulesOpenApiDocHelper(const VmsRulesOpenApiDocHelper&) = delete;
    VmsRulesOpenApiDocHelper& operator=(const VmsRulesOpenApiDocHelper&) = delete;

    /**
     * Changes the input string to the appropriate schema name format.
     * Input example : "analyticsObject"
     * Result example: "AnalyticsObject"
     */
    static QString schemaName(const QString& id);
    static QJsonObject generateOpenApiDoc(nx::vms::common::SystemContext* context);

    /**
     * Adds schemas and paths for VmsRules that are missing in basicDoc.
     * In case of duplication, the paths from generated documentation overwrites the one in basicDocFile.
     * If basicDoc does not require VmsRules documentation, the original document will be returned.
     */
    QJsonObject addVmsRulesDocIfRequired(const QString& basicDocFile,
        nx::vms::common::SystemContext* systemContext);

private:
    VmsRulesOpenApiDocHelper() = default;
    ~VmsRulesOpenApiDocHelper() = default;
    nx::Lockable<std::map<QString, QJsonObject>> m_cache;
};
}  // namespace nx::vms::rules::utils
