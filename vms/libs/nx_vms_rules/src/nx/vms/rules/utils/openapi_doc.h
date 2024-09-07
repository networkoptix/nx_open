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

static constexpr auto kDocOpenApiSchemePropertyName = "openApiSchema";
const int kMinimumVersionRequiresMerging = 4;

template<class FieldType>
static QVariantMap addOpenApiToProperties(QVariantMap properties)
{
    auto visibleIt = properties.find("visible");
    if (visibleIt != properties.end() && !visibleIt->toBool())
        return properties; //< Ignoring not visible in UI fields.

    properties[kDocOpenApiSchemePropertyName] = FieldType::openApiDescriptor();
    return properties;
}

NX_VMS_RULES_API QJsonObject getPropertyOpenApiDescriptor(const QMetaType& metatype);

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

NX_VMS_RULES_API void updatePropertyForField(QJsonObject& propertiesObject,
    const QString& fieldName,
    const QString& docPropertyName,
    const QString& value);

class NX_VMS_RULES_API VmsRulesOpenApiDocHelper
{
public:
    static VmsRulesOpenApiDocHelper& getInstance() {
        static VmsRulesOpenApiDocHelper instance;
        return instance;
    }

    VmsRulesOpenApiDocHelper(const VmsRulesOpenApiDocHelper&) = delete;
    VmsRulesOpenApiDocHelper& operator=(const VmsRulesOpenApiDocHelper&) = delete;

    /**
     * Changes the input string to the appropriate schema name format.
     * Input example : "nx.events.analyticsObject"
     * Result example: "AnalyticsObjectEvent"
     */
    static QString schemaName(const QString& id);
    static QJsonObject generateOpenApiDoc(Engine* engine);

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
