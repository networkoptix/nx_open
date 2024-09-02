// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "openapi_doc.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/field_types.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/rules_fwd.h>

namespace {

/**
 * Changes the input string to start with a capital letter and removes the last letter 's' if present.
 * Input examples: "events", "analyticsObject"
 * Result examples: "Event", "AnalyticObject"
 */
QString formatIdPart(QString part)
{
    part[0] = part[0].toUpper();
    if (part.endsWith('s'))
        part.removeLast();
    return part;
}

/**
 * Changes the input string to the appropriate schema name format.
 * Input example : "nx.events.analyticsObject"
 * Result example: "AnalyticsObjectEvent"
 */
QString schemaName(const QString& id)
{
    const auto parts = id.split('.');
    if (parts.size() < 2)
    {
        NX_ASSERT("Incorrect id: %1", id);
        return id;
    }
    return formatIdPart(parts.last()) + formatIdPart(parts[parts.size() - 2]);
}

QJsonObject constructSchemaReference(const QString& displayName)
{
    QJsonObject reference;
    reference["$ref"] = "#/components/schemas/" + schemaName(displayName);
    return reference;
}

QJsonObject createTypeDescriptor(const QString& id)
{
    const auto lastPartOfId = id.split('.').last();
    return QJsonObject{{"type", "string"}, {"enum", QJsonArray({lastPartOfId})}};
}
} // namespace

namespace nx::vms::rules::utils {

void collectOpenApiInfo(
    const QMap<QString, ItemDescriptor>& items, QJsonObject& schemas, QJsonArray& listReference)
{
    for (const auto& item: items)
    {
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject properties;
        for (const auto& field: item.fields)
        {
            if (!field.properties.contains(kDocOpenApiSchemePropertyName))
                continue; //< Field is invisible or doesn't require documentation.

            QJsonObject fieldObject =
                field.properties[kDocOpenApiSchemePropertyName].toJsonObject();
            if (fieldObject.isEmpty())
                NX_ASSERT("Lack of documentation for the type: %1", field.id);

            properties[field.fieldName] = fieldObject;
        }
        if (properties.empty() && !item.fields.empty())
        {
            NX_ASSERT("No generated documentation for item %1", item.id);
            continue;
        }

        properties["type"] = createTypeDescriptor(item.id);
        schema["properties"] = properties;
        listReference.push_back(constructSchemaReference(item.id));
        schemas[schemaName(item.id)] = schema;
    }
}

template<typename T>
QJsonObject createEnumDescriptor()
{
    QJsonObject result;
    result["type"] = "string";
    QJsonArray possibleValues;

    reflect::enumeration::visitAllItems<T>(
        [&](auto&&... items)
        {
            for (auto value: {items.value...})
                possibleValues.append(QString::fromStdString(reflect::toString<T>(value)));
        });

    result["enum"] = possibleValues;
    result["example"] = possibleValues.first();
    return result;
}

QJsonObject getPropertyOpenApiDescriptor(const QMetaType& metaType)
{
    using namespace nx::vms::api;
    QJsonObject result;

    if (metaType == QMetaType::fromType<UuidSet>() || metaType == QMetaType::fromType<UuidList>())
    {
        result["type"] = "array";
        result["items"] = QJsonObject{{"type", "string"}, {"format", "uuid"}};
        result["uniqueItems"] = metaType == QMetaType::fromType<UuidSet>();
        result["example"] = QJsonArray({"{00000000-0000-0000-0000-000000000000}"});
        return result;
    }

    if (metaType == QMetaType::fromType<Uuid>())
    {
        result["type"] = "string";
        result["format"] = "uuid";
        result["example"] = "{00000000-0000-0000-0000-000000000000}";
        return result;
    }

    if (metaType == QMetaType::fromType<vms::api::rules::State>())
        return createEnumDescriptor<vms::api::rules::State>();

    if (metaType == QMetaType::fromType<ObjectLookupCheckType>())
        return createEnumDescriptor<ObjectLookupCheckType>();

    if (metaType == QMetaType::fromType<network::http::AuthType>())
        return createEnumDescriptor<network::http::AuthType>();

    if (metaType == QMetaType::fromType<TextLookupCheckType>())
        return createEnumDescriptor<TextLookupCheckType>();

    if (metaType == QMetaType::fromType<EventLevels>())
    {
        auto result = createEnumDescriptor<EventLevel>();
        result["description"] =
            "The value can contain either a single Event Level "
            "or a combination of levels. "
            "To create a combination, use the | symbol. <b>For example: info|warning.</b>";
        return result;
    }

    if (metaType == QMetaType::fromType<StreamQuality>())
        return createEnumDescriptor<StreamQuality>();

    if (metaType == QMetaType::fromType<std::chrono::microseconds>())
    {
        result["type"] = "string";
        result["example"] = "10000";
        result["description"] = "Length in microseconds";
        return result;
    }

    switch (metaType.id())
    {
        case QMetaType::Bool:
        {
            result["type"] = "boolean";
            result["example"] = false;
            return result;
        }

        case QMetaType::Int:
        {
            result["type"] = "integer";
            result["example"] = 123;
            return result;
        }

        case QMetaType::Float:
        {
            result["type"] = "number";
            result["format"] = "float";
            result["example"] = 12.3;
            return result;
        }

        default:
        {
            if (metaType != QMetaType::fromType<QString>())
                NX_ASSERT(false, "Not supported field type: %1", metaType.name());

            result["type"] = "string";
            result["example"] = "example";
        }
    }

    return result;
}

void createEventRulesOpenApiDoc(Engine* engine)
{
    // TODO: after VMS-53813, full response and request should be generated based on struct RuleV4.
    // For now some fields(enabled/schedule) are writen manually in template
    QFile templateOpenApi(":/vms_rules_templates/template_event_rules_openapi.json.in");
    if (!templateOpenApi.open(QFile::ReadOnly))
        return;

    QTextStream textStream(&templateOpenApi);
    QString templateText = textStream.readAll();

    QJsonObject schemas;
    QJsonArray eventListReference;
    collectOpenApiInfo(engine->events(), schemas, eventListReference);

    QJsonArray actionListReference;
    collectOpenApiInfo(engine->actions(), schemas, actionListReference);

    QString path = QCoreApplication::applicationDirPath() + QString("/event_rules_openapi.json");
    QFile out(path);
    out.open(QFile::WriteOnly);
    if (!out.isOpen())
        return;

    const auto jsonActionList = QJsonDocument(actionListReference).toJson();
    const auto jsonEventList = QJsonDocument(eventListReference).toJson();
    QString jsonSchemas = QJsonDocument(schemas).toJson();

    // Remove closing brackets
    jsonSchemas = jsonSchemas.trimmed().removeFirst().removeLast();
    templateText = templateText.arg(
        jsonActionList, jsonEventList, jsonSchemas.toUtf8(), jsonActionList, jsonEventList);
    out.write(QJsonDocument::fromJson(templateText.toUtf8()).toJson());
}

} // namespace nx::vms::rules::utils
