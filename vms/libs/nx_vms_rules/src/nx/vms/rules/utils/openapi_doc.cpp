// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "openapi_doc.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <nx/utils/json.h>
#include <nx/utils/lockable.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/common/system_context.h>
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

QJsonObject createTypeDescriptor(const QString& id)
{
    const auto lastPartOfId = id.split('.').last();
    return QJsonObject{{"type", "string"}, {"enum", QJsonArray({lastPartOfId})}};
}

QJsonObject readOpenApiDoc(const QString& docPath)
{
    QFile file(docPath);
    if (!file.open(QFile::ReadOnly))
        return {};

    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(file.readAll(), &jsonError);

    if (!NX_ASSERT(jsonError.error == QJsonParseError::NoError,
            "Failed to parse JSON: %1",
            jsonError.errorString()))
    {
        return {};
    }

    if (!NX_ASSERT(jsonDoc.isObject(), "JSON is not an object."))
        return {};

    return jsonDoc.object();
}

template<typename T>
QJsonObject createEnumDescriptor()
{
    QJsonObject result;
    result["type"] = "string";
    QJsonArray possibleValues;

    nx::reflect::enumeration::visitAllItems<T>(
        [&](auto&&... items)
        {
            for (auto value: {items.value...})
                possibleValues.append(QString::fromStdString(nx::reflect::toString<T>(value)));
        });

    result["enum"] = possibleValues;
    result["example"] = possibleValues.first();
    return result;
}

} // namespace

namespace nx::vms::rules::utils {
const QString kTemplatePath = ":/vms_rules_templates/template_event_rules_openapi.json.in";

QJsonObject constructSchemaReference(const QString& displayName)
{
    QJsonObject reference;
    reference["$ref"] = "#/components/schemas/" + VmsRulesOpenApiDocHelper::schemaName(displayName);
    return reference;
}

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

            if (!NX_ASSERT(!fieldObject.isEmpty(),
                    "Lack of documentation for the type: %1. In item %2",
                    field.id,
                    item.id))
            {
                continue;
            }

            const auto apiFieldName = toApiFieldName(field.fieldName, field.id);
            properties[apiFieldName] = fieldObject;
        }

        const bool isDocGenerated = !properties.empty() || item.fields.empty();
        if (!NX_ASSERT(isDocGenerated, "No generated documentation for item %1", item.id))
            continue;

        properties["type"] = createTypeDescriptor(item.id);
        schema["properties"] = properties;
        listReference.push_back(constructSchemaReference(item.id));
        schemas[VmsRulesOpenApiDocHelper::schemaName(item.id)] = schema;
    }
}


QString VmsRulesOpenApiDocHelper::schemaName(const QString& id)
{
    const auto parts = id.split('.');
    if (!NX_ASSERT(parts.size() >= 2,"Incorrect id: %1", id))
        return id;

    return formatIdPart(parts.last()) + formatIdPart(parts[parts.size() - 2]);
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
        result["type"] = "integer";
        result["example"] = 10;
        // API sends value in seconds, but it is stored as microseconds in classes.
        result["description"] = "Length in seconds";
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

QJsonObject VmsRulesOpenApiDocHelper::generateOpenApiDoc(Engine* engine)
{
    QFile templateOpenApi(kTemplatePath);
    if (!NX_ASSERT(
            templateOpenApi.open(QFile::ReadOnly), "The OpenAPI doc template file cannot be opened."))
    {
        return {};
    }

    QTextStream textStream(&templateOpenApi);
    QString templateText = textStream.readAll();

    QJsonObject schemas;
    QJsonArray eventListReference;
    collectOpenApiInfo(engine->events(), schemas, eventListReference);

    QJsonArray actionListReference;
    collectOpenApiInfo(engine->actions(), schemas, actionListReference);

    const auto jsonActionList = QJsonDocument(actionListReference).toJson();
    const auto jsonEventList = QJsonDocument(eventListReference).toJson();
    QString jsonSchemas = QJsonDocument(schemas).toJson();

    // Remove closing brackets.
    jsonSchemas = jsonSchemas.trimmed().removeFirst().removeLast();
    templateText = templateText.arg(
        jsonActionList, jsonEventList, jsonSchemas.toUtf8(), jsonActionList, jsonEventList);

    return QJsonDocument::fromJson(templateText.toUtf8()).object();
}

QJsonObject getSchemasObject(const QJsonObject& openApiDocObject)
{
    using namespace nx::utils::json;
    return optObject(
        optObject(openApiDocObject, "components", "No element component in openapi doc"),
        "schemas",
        "No element schemas in openapi doc");
}

QJsonObject getPathsObject(const QJsonObject& openApiDocObject)
{
    using namespace nx::utils::json;
    return optObject(openApiDocObject, "paths", "No element paths in openapi doc");
}


void mergeOpenApiDoc(QJsonObject& basicDocToMergeInto, const QJsonObject& vmsRulesDoc)
{
    if (!NX_ASSERT(!vmsRulesDoc.isEmpty() && !basicDocToMergeInto.isEmpty(),
            "Incorrect documentation for merging"))
    {
        return;
    }

    using namespace nx::utils::json;
    auto originalSchemas = getSchemasObject(basicDocToMergeInto);

    auto vmsSchemas = getSchemasObject(vmsRulesDoc);
    for(auto& vmsSchemaName : vmsSchemas.keys())
        originalSchemas[vmsSchemaName] = std::move(vmsSchemas.take(vmsSchemaName));

    auto originalPaths = getPathsObject(basicDocToMergeInto);
    auto vmsRulesPaths = getPathsObject(vmsRulesDoc);
    for(const auto& pathName : vmsRulesPaths.keys())
        originalPaths[pathName] = std::move(vmsRulesPaths.take(pathName));

    auto originalComponents = optObject(basicDocToMergeInto,"components");
    originalComponents["schemas"] = originalSchemas;

    basicDocToMergeInto["components"] = originalComponents;
    basicDocToMergeInto["paths"] = originalPaths;
}

QJsonObject VmsRulesOpenApiDocHelper::addVmsRulesDocIfRequired(
    const QString& basicDocFilePath, nx::vms::common::SystemContext* systemContext)
{
    auto openApiDoc = readOpenApiDoc(basicDocFilePath);
    if (openApiDoc.isEmpty())
        return {};

    // Get version from basicDocFilePath.
    static const QRegularExpression rx("[^0-9]+");
    const auto parts = basicDocFilePath.split(rx, Qt::SkipEmptyParts);

    if (parts.isEmpty() || parts.first().toInt() < kMinimumVersionRequiresMerging)
        return openApiDoc; //< OpenAPI doc doesn't require merging.

    auto lockableResource = m_cache.lock();
    auto it = lockableResource->find(basicDocFilePath);
    if (it != lockableResource->end())
        return it->second;

    mergeOpenApiDoc(openApiDoc, generateOpenApiDoc(systemContext->vmsRulesEngine()));

    lockableResource->emplace(basicDocFilePath, openApiDoc);
    return openApiDoc;
}

void updatePropertyForField(QJsonObject& propertiesObject,
    const QString& fieldName,
    const QString& docPropertyName,
    const QString& value)
{
    auto fieldJson = propertiesObject[fieldName].toObject();
    fieldJson[docPropertyName] = value;
    propertiesObject[fieldName] = fieldJson;
}

} // namespace nx::vms::rules::utils
