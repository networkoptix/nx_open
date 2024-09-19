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
#include <nx/vms/rules/common/time_field_properties.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/field_types.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/rules_fwd.h>

namespace {

struct PropertyDescriptor
{
    QString type;
    QVariant example;
    QString format;
    QString description;
};

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

QJsonValue generateDefaultValue(const QMetaType& metaType)
{
    QVariant defaultValue(metaType);

    if (metaType.isValid())
    {
        metaType.construct(defaultValue.data());
        return QJsonValue::fromVariant(defaultValue);
    }

    return {};
}

PropertyDescriptor mapMetaTypeToJsonTypeAndExample(const QMetaType& metaType)
{
    static const QMap<int, PropertyDescriptor> metaTypeToDescriptor = {
        {QMetaType::Bool, {.type = "boolean", .example = true}},
        {QMetaType::Int, {.type = "integer", .example = 123}},
        {QMetaType::UInt, {.type = "integer", .example = 123}},
        {QMetaType::Float, {.type = "number", .example = 12.3f, .format = "float"}},
        {QMetaType::Double, {.type = "number", .example = 12.3, .format = "double"}},
        {QMetaType::QString, {.type = "string", .example = "example string"}},
        // API sends value in seconds, but it is stored as microseconds in classes.
        {QMetaType::fromType<std::chrono::microseconds>().id(),
            {.type = "integer", .example = 10, .description = "Length in seconds"}},
        {QMetaType::fromType<nx::Uuid>().id(),
            {.type = "string",
                .example = "{00000000-0000-0000-0000-000000000000}",
                .format = "uuid"}},
    };

    auto it = metaTypeToDescriptor.find(metaType.id());
    if (!NX_ASSERT(it != metaTypeToDescriptor.end(),
            "Not supported field type: %1",
            metaType.name()))
    {
        return {};
    }

    return it.value();
}

} // namespace

namespace nx::vms::rules::utils {
const QString kTemplatePath = ":/vms_rules_templates/template_event_rules_openapi.json.in";

QJsonObject createTypeDescriptor(const QString& id)
{
    const auto lastPartOfId = id.split('.').last();
    return QJsonObject{{kTypeProperty, "string"}, {kEnumProperty, QJsonArray({lastPartOfId})}};
}

void appendFieldDescription(QJsonObject& fieldObject, const FieldDescriptor& field)
{
    if (field.description.isEmpty())
        return;
    const auto basicDescription = fieldObject[kDescriptionProperty].toString();
    fieldObject[kDescriptionProperty] = field.description + "<br/>" + basicDescription;
}

QJsonObject createUuidListDescriptor(bool isSet)
{
    QJsonObject result;
    result[kTypeProperty] = "array";
    result["items"] = QJsonObject{{kTypeProperty, "string"}, {"format", "uuid"}};
    result["uniqueItems"] = isSet;
    result[kExampleProperty] = QJsonArray({"{00000000-0000-0000-0000-000000000000}"});
    return result;
}

QJsonObject createBasicTypeDescriptor(const QMetaType& metaType, bool addDefaultValue)
{
    QJsonObject result;

    auto descriptor = mapMetaTypeToJsonTypeAndExample(metaType);
    if (descriptor.type.isEmpty())
        return {};

    result[kTypeProperty] = descriptor.type;
    result[kExampleProperty] = QJsonValue::fromVariant(descriptor.example);

    if (!descriptor.format.isEmpty())
        result["format"] = descriptor.format;

    if (!descriptor.description.isEmpty())
        result[kDescriptionProperty] = descriptor.description;

    if (addDefaultValue)
        result[kDefaultProperty] = generateDefaultValue(metaType);

    return result;
}

template<typename T>
QJsonObject createEnumDescriptor(bool addDefault)
{
    QJsonObject result;
    result[kTypeProperty] = "string";
    QJsonArray possibleValues;

    nx::reflect::enumeration::visitAllItems<T>(
        [&](auto&&... items)
        {
            for (auto value: {items.value...})
                possibleValues.append(QString::fromStdString(nx::reflect::toString<T>(value)));
        });

    result[kEnumProperty] = possibleValues;
    result[kExampleProperty] = possibleValues.at(possibleValues.size() / 2);
    if (addDefault)
        result[kDefaultProperty] = QString::fromStdString(nx::reflect::toString<T>(T()));
    return result;
}

template<typename T, typename... Enums>
QJsonObject handleEnumTypes(const QMetaType& metaType, bool addDefaultValue)
{
    // Check if the first type in the parameter pack is an enum and matches the metaType.
    if constexpr (std::is_enum_v<T>)
    {
        if (metaType == QMetaType::fromType<T>())
            return createEnumDescriptor<T>(addDefaultValue);
    }

    // Recursively check the rest of the parameter pack.
    if constexpr (sizeof...(Enums) > 0)
    {
        return handleEnumTypes<Enums...>(metaType, addDefaultValue);
    }

    return {};
}

QJsonObject createStreamQualityDescriptor(bool addDefaultValue)
{
    using namespace nx::vms::api;
    auto defaultValues = createEnumDescriptor<StreamQuality>(addDefaultValue);
    auto enumVals = defaultValues[kEnumProperty].toArray();
    static QSet<QString> kIgnoredValue{
        QString::fromStdString(reflect::toString(StreamQuality::undefined)),
        QString::fromStdString(reflect::toString(StreamQuality::rapidReview)),
        QString::fromStdString(reflect::toString(StreamQuality::preset))};

    for (int i = enumVals.size() - 1; i >= 0; --i)
    {
        const QJsonValue& val = enumVals.at(i);
        if (!NX_ASSERT(val.isString(), "Enum values must be string"))
            continue;

        if (kIgnoredValue.contains(val.toString()))
            enumVals.removeAt(i);
    }

    defaultValues[kEnumProperty] = enumVals;
    return defaultValues;
}

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
        if (item.flags.testFlag(ItemFlag::system))
            continue;

        QJsonObject schema;
        schema[kTypeProperty] = "object";
        QJsonObject properties;
        // Field type is required for all events and actions.
        QJsonArray requiredFields{kTypeProperty};
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

            appendFieldDescription(fieldObject, field);

            const auto apiFieldName = toApiFieldName(field.fieldName, field.id);
            properties[apiFieldName] = fieldObject;

            auto optionalIt = field.properties.find(FieldProperties::kIsOptionalFieldPropertyName);
            if (optionalIt != field.properties.end() && !optionalIt->toBool())
                requiredFields.append(apiFieldName);
        }

        const bool isDocGenerated = !properties.empty() || item.fields.empty();
        if (!NX_ASSERT(isDocGenerated, "No generated documentation for item %1", item.id))
            continue;

        properties[kTypeProperty] = createTypeDescriptor(item.id);
        schema[kPropertyKey] = properties;
        schema["required"] = requiredFields;
        if (!item.description.isEmpty())
            schema[kDescriptionProperty] = item.description;
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

QJsonObject addAdditionalDocFromFieldProperties(
    QJsonObject openApiDescriptor, const QVariantMap& fieldProperties)
{
    if (fieldProperties.empty())
        return openApiDescriptor;

    auto updateDescriptorWithProperties =
        [&openApiDescriptor]
        (const QVariantMap& properties, const QSet<QString>& propertyKeys, const QString& docKey)
        {
            auto keyIt = std::find_if(
                propertyKeys.cbegin(), propertyKeys.cend(),
            [&properties](const QString& key)
                 {
                    return properties.contains(key);
                 });

            if (keyIt == propertyKeys.end())
                return; //< There is no expected properties in field.

            auto it = properties.find(*keyIt);

            // Have to explicitly check for bool, otherwise the value will be converted to a number.
            if (it->metaType() == QMetaType::fromType<bool>())
            {
                openApiDescriptor[docKey] = it->toBool();
                return;
            }

            if (it->metaType() == QMetaType::fromType<std::chrono::seconds>())
            {
                // A cast is required to avoid ambiguity during conversion
                // to const QJsonValue on GCC.
                openApiDescriptor[docKey] =
                    static_cast<qint64>(it->value<std::chrono::seconds>().count());
                return;
            }

            if (it->metaType() == QMetaType::fromType<vms::api::rules::State>())
            {
                openApiDescriptor[docKey] =
                    QString::fromStdString(reflect::toString(it->value<vms::api::rules::State>()));
                return;
            }

            auto value = QJsonValue::fromVariant(*it);

            if (value.isDouble() && openApiDescriptor.contains(kEnumProperty))
            {
                NX_ASSERT(false, "The enum value was incorrectly converted to a numeric value");
                return;
            }

            openApiDescriptor[docKey] = QJsonValue::fromVariant(*it);
        };

    updateDescriptorWithProperties(fieldProperties, {"value", "text"}, kDefaultProperty);
    updateDescriptorWithProperties(fieldProperties, {"min"}, kMinProperty);
    updateDescriptorWithProperties(fieldProperties, {"max"}, kMaxProperty);

    return openApiDescriptor;
}

QJsonObject getPropertyOpenApiDescriptor(const QMetaType& metaType, bool addDefaultValue)
{
    using namespace nx::vms::api;

    if (metaType == QMetaType::fromType<UuidSet>() || metaType == QMetaType::fromType<UuidList>())
        return createUuidListDescriptor(metaType == QMetaType::fromType<UuidSet>());

    if (auto enumDescriptor = handleEnumTypes<vms::api::rules::State,
            ObjectLookupCheckType,
            network::http::AuthType,
            TextLookupCheckType>(metaType, addDefaultValue);
        !enumDescriptor.isEmpty())
    {
        return enumDescriptor;
    }

    if (metaType == QMetaType::fromType<EventLevels>())
    {
        QJsonObject result = createEnumDescriptor<EventLevel>(addDefaultValue);
        result[kDescriptionProperty] =
            "The value can contain either a single Event Level "
            "or a combination of levels. "
            "To create a <b>combination</b>, use the <code>|</code> symbol. "
            "For example: <code>info|warning.</code>";
        return result;
    }

    if (metaType == QMetaType::fromType<StreamQuality>())
        return createStreamQualityDescriptor(addDefaultValue);

    return createBasicTypeDescriptor(metaType, addDefaultValue);
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

void updatePropertyForField(QJsonObject& openApiObjectDescriptor,
    const QString& fieldName,
    const QString& docPropertyName,
    const QString& value)
{
    using namespace nx::utils::json;
    auto properties =
        optObject(openApiObjectDescriptor, kPropertyKey, "openApiDescriptor lacks properties");

    auto fieldJson =
        optObject(properties, fieldName, QString("properties lacks %1").arg(fieldName));
    if (fieldJson.isEmpty())
        return;
    fieldJson[docPropertyName] = value;
    properties[fieldName] = fieldJson;
    openApiObjectDescriptor[kPropertyKey] = properties;
}

} // namespace nx::vms::rules::utils
