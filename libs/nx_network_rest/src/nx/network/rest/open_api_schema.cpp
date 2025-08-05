// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "open_api_schema.h"

#include <deque>

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <nx/utils/json.h>

#include "array_orderer.h"
#include "request.h"

namespace nx::network::rest::json {

namespace {

using namespace nx::utils::json;
using namespace nx::network::rest;

QString escapeName(const QString& name)
{
    static const QByteArray kInclude = ".";
    static const QByteArray kExclude;
    return QUrl::toPercentEncoding(name, kExclude, kInclude);
}

QString unescapeName(const QString& name)
{
    return QUrl::fromPercentEncoding(name.toUtf8());
}

std::string unescapeName(const std::string& name)
{
    return QUrl::fromPercentEncoding(QByteArray(name)).toStdString();
}

void merge(QJsonObject* to, QJsonObject from)
{
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        const auto& key = it.key();
        auto value = it.value();
        if (const auto property = to->find(key); property != to->end())
        {
            QJsonObject valueObject = value.toObject();
            if (!NX_ASSERT(!valueObject.isEmpty()))
                return;
            QJsonObject object = property->toObject();
            if (!NX_ASSERT(!object.isEmpty()))
                return;
            merge(&object, std::move(valueObject));
            (*to)[key] = std::move(object);
        }
        else
        {
            (*to)[key] = value;
        }
    }
}

class Postprocessor
{
public:
    Postprocessor(ArrayOrderer&& orderer): m_orderer(std::move(orderer)) {}

    bool operator()(QJsonValue* origin, const QJsonObject& schema)
    {
        if (schema.isEmpty())
            return false;

        const auto originType = origin->type();
        if (originType != QJsonValue::Array && originType != QJsonValue::Object)
            return false;

        const auto type = schema.find("type");
        if (type == schema.end())
        {
            const auto oneOf = schema.find("oneOf");
            if (oneOf == schema.end())
                return false;

            for (auto item: asArray(oneOf.value()))
            {
                const auto itemSchema = asObject(item);
                if (itemSchema.isEmpty())
                    continue;

                const auto itemType = optString(itemSchema, "type");
                //Do not process oneOf objects as fields can be erased mistakenly.
                if (originType == QJsonValue::Object && itemType == "object")
                    return false;

                if ((itemType.isEmpty() && itemSchema.contains("oneOf"))
                    || (originType == QJsonValue::Array && itemType == "array"))
                {
                    return (*this)(origin, itemSchema);
                }
            }

            NX_ASSERT(false, "No %1 in oneOf %2",
                originType == QJsonValue::Array ? "array" : "object");
            return false;
        }

        if (originType == QJsonValue::Array)
        {
            if (!NX_ASSERT(type->toString() == "array"))
                return false;

            const auto items = schema.find("items");
            if (!NX_ASSERT(items != schema.end()))
                return false;
            return postprocessArray(origin, items->toObject());
        }
        else
        {
            if (!NX_ASSERT(type->toString() == "object"))
                return false;

            if (const auto additionalSchema = optObject(schema, "additionalProperties");
                !additionalSchema.isEmpty())
            {
                bool postprocessed = false;
                auto originObject = origin->toObject();
                for (auto field = originObject.begin(); field != originObject.end(); ++field)
                {
                    const QString fieldName = field->isArray() ? "*[]" : "*";
                    const ArrayOrderer::FieldScope scope(fieldName, &m_orderer);
                    QJsonValue value = field.value();
                    if ((*this)(&value, additionalSchema))
                    {
                        postprocessed = true;
                        field.value() = value;
                    }
                }
                if (postprocessed)
                    *origin = originObject;
                return postprocessed;
            }

            const auto properties = schema.find("properties");
            if (properties == schema.end())
                return false;
            const auto& propertiesObject = properties->toObject();
            if (!NX_ASSERT(!propertiesObject.isEmpty()))
                return false;
            return postprocessObject(origin, propertiesObject);
        }
    }

private:
    bool postprocessArray(QJsonValue* origin, const QJsonObject& items)
    {
        bool postprocessed = false;
        QJsonArray output;
        QJsonArray originArray = origin->toArray();
        for (QJsonValue value: originArray)
        {
            if ((*this)(&value, items))
                postprocessed = true;
            output.append(value);
        }
        if (postprocessed)
        {
            m_orderer(&output);
            *origin = output;
        }
        else
        {
            if (m_orderer(&originArray))
            {
                *origin = originArray;
                postprocessed = true;
            }
        }
        return postprocessed;
    }

    bool postprocessObject(QJsonValue* origin, const QJsonObject& properties)
    {
        bool postprocessed = false;
        auto originObject = origin->toObject();
        for (auto field = originObject.begin(); field != originObject.end();)
        {
            const QString fieldName = field->isArray() ? (field.key() + "[]") : field.key();
            const ArrayOrderer::FieldScope scope(fieldName, &m_orderer);
            if (const auto property = properties.find(field.key()); property != properties.end())
            {
                QJsonValue value = field.value();
                if ((*this)(&value, property->toObject()))
                {
                    postprocessed = true;
                    field.value() = value;
                }
                ++field;
            }
            else
            {
                postprocessed = true;
                originObject.erase(field);
            }
        }
        if (postprocessed)
            *origin = originObject;
        return postprocessed;
    }

private:
    nx::network::rest::json::ArrayOrderer m_orderer;
};

class PostprocessorV4
{
public:
    bool operator()(rapidjson::Value* origin, const QJsonObject& schema)
    {
        if (schema.isEmpty())
            return false;

        const auto originType = origin->GetType();
        if (originType != rapidjson::kArrayType && originType != rapidjson::kObjectType)
            return false;

        const auto type = schema.find("type");
        if (type == schema.end())
        {
            const auto oneOf = schema.find("oneOf");
            if (oneOf == schema.end())
                return false;

            for (auto item: asArray(oneOf.value()))
            {
                const auto itemSchema = asObject(item);
                if (itemSchema.isEmpty())
                    continue;

                const auto itemType = optString(itemSchema, "type");
                //Do not process oneOf objects as fields can be erased mistakenly.
                if (originType == rapidjson::kObjectType && itemType == "object")
                    return false;

                if ((itemType.isEmpty() && itemSchema.contains("oneOf"))
                    || (originType == rapidjson::kArrayType && itemType == "array"))
                {
                    return (*this)(origin, itemSchema);
                }
            }

            NX_ASSERT(false, "No %1 in oneOf %2",
                originType == rapidjson::kArrayType ? "array" : "object");
            return false;
        }

        if (originType == rapidjson::kArrayType)
        {
            if (!NX_ASSERT(type->toString() == "array"))
                return false;

            const auto items = schema.find("items");
            if (!NX_ASSERT(items != schema.end()))
                return false;
            return postprocessArray(origin, items->toObject());
        }
        else
        {
            if (!NX_ASSERT(type->toString() == "object"))
                return false;

            if (const auto additionalSchema = optObject(schema, "additionalProperties");
                !additionalSchema.isEmpty())
            {
                bool postprocessed = false;
                for (auto it = origin->MemberBegin(); it != origin->MemberEnd(); ++it)
                {
                    const QString fieldName = it->value.IsArray() ? "*[]" : "*";
                    if ((*this)(&it->value, additionalSchema))
                        postprocessed = true;
                }
                return postprocessed;
            }

            const auto properties = schema.find("properties");
            if (properties == schema.end())
                return false;
            const auto& propertiesObject = properties->toObject();
            if (!NX_ASSERT(!propertiesObject.isEmpty()))
                return false;
            return postprocessObject(origin, propertiesObject);
        }
    }

private:
    bool postprocessArray(rapidjson::Value* origin, const QJsonObject& items)
    {
        bool postprocessed = false;
        for (rapidjson::SizeType i = 0; i < origin->Size(); ++i)
        {
            if ((*this)(&(*origin)[i], items))
                postprocessed = true;
        }
        return postprocessed;
    }

    bool postprocessObject(rapidjson::Value* origin, const QJsonObject& properties)
    {
        bool postprocessed = false;
        for (auto it = origin->MemberBegin(); it != origin->MemberEnd();)
        {
            const QString fieldName{it->name.GetString()};
            if (const auto property = properties.find(fieldName); property != properties.end())
            {
                if ((*this)(&it->value, property->toObject()))
                    postprocessed = true;
                ++it;
            }
            else
            {
                postprocessed = true;
                it = origin->EraseMember(it);
            }
        }
        return postprocessed;
    }
};

void validateParameters(const QJsonObject& schema,
    const QJsonValue& value,
    const QString& name,
    std::vector<QString>* unused);

void validateParameters(const QJsonObject& schema,
    const QJsonArray& value,
    const QString& name,
    std::vector<QString>* unused)
{
    const auto items = getObject(schema, "items");
    if (items.isEmpty())
        return;
    for (size_t i = 0, n = value.size(); i < n; ++i)
        validateParameters(items, value[i], name + '[' + nx::toString(i) + ']', unused);
}

void validateParameters(const QJsonObject& schema,
    const QJsonObject& value,
    const QString& name,
    std::vector<QString>* unused)
{
    const auto properties = optObject(schema, "properties", name);
    auto required = optArray(schema, "required", name);
    for (auto field = value.begin(); field != value.end(); ++field)
    {
        for (auto requiredValue = required.begin(); requiredValue != required.end();
             ++requiredValue)
        {
            const QString requiredName = requiredValue->toString();
            if (!NX_ASSERT(!requiredName.isEmpty(), name))
                continue;
            if (requiredName == field.key())
            {
                required.erase(requiredValue);
                break;
            }
        }
        const auto fieldName =
            name.isEmpty() ? escapeName(field.key()) : name + '.' + escapeName(field.key());
        if (const auto fieldSchema = properties.find(field.key()); fieldSchema != properties.end())
        {
            const QJsonObject fieldObj = fieldSchema->toObject();
            if (optBool(fieldObj, "readOnly", fieldName))
                unused->push_back(fieldName);
            else
                validateParameters(fieldObj, field.value(), fieldName, unused);
        }
        // TODO: Input node should not be `unused` if it is allowed by `additionalProperties`.
        else if (!properties.empty())
        {
            unused->push_back(fieldName);
        }
    }
    for (const auto& requiredValue: required)
    {
        const QString requiredName = requiredValue.toString();
        if (NX_ASSERT(!requiredName.isEmpty(), name))
        {
            throw Exception::missingParameter(
                name.isEmpty() ? requiredName : unescapeName(name) + '.' + requiredName);
        }
    }
}

void validateParameters(const QJsonObject& schema,
    const QJsonValue& value,
    const QString& name,
    std::vector<QString>* unused)
{
    if (schema.isEmpty())
        return;
    QJsonObject additionalSchema;
    QString typeName;
    const auto nameForError =
        [&name] { return name.isEmpty() ? QString("Request body") : unescapeName(name); };
    if (const auto type = schema.find("type"); type == schema.end())
    {
        additionalSchema = optObject(schema, "additionalProperties", nameForError());
        if (additionalSchema.isEmpty())
            return;
    }
    else
    {
        typeName = type->toString();
    }

    // TODO: Check for `nullable: true`.
    //   OpenAPI spec allows for the value to be `null` if specified:
    //   https://swagger.io/docs/specification/data-models/data-types/#null
    if (value.isNull())
        throw Exception::invalidParameter(nameForError(), "null");
    if (typeName.isEmpty() || typeName == "object")
    {
        if (!value.isObject())
            throw Exception::badRequest(NX_FMT("`%1` must be an object.", nameForError()));
        if (!typeName.isEmpty())
            additionalSchema = optObject(schema, "additionalProperties", nameForError());
        const auto object = value.toObject();

        // TODO:
        //   OpenAPI allows to specify `properties` and `additionalProperties` at the same time.
        //   However, `properties` takes precedence `additionalProperties`.
        //   Meaning that the order for validation is:
        //      - check against `properties`
        //      - check against `additionalProperties`
        //      - move to `unused`
        if (!additionalSchema.isEmpty())
        {
            for (auto field = object.begin(); field != object.end(); ++field)
            {
                QString keyName = escapeName(field.key());
                validateParameters(
                    additionalSchema,
                    field.value(),
                    name.isEmpty() ? keyName : name + '.' + keyName,
                    unused);
            }
            return;
        }
        validateParameters(schema, object, name, unused);
    }
    else if (typeName == "array")
    {
        if (!value.isArray())
            throw Exception::badRequest(NX_FMT("`%1` must be an array.", nameForError()));
        validateParameters(schema, value.toArray(), name, unused);
    }
    else if (typeName == "string")
    {
        if (!value.isString())
            throw Exception::badRequest(NX_FMT("`%1` must be a string.", nameForError()));
        // TODO: Check for the format of the actual values here.
    }
    else if (typeName == "number")
    {
        if (!value.isDouble())
            throw Exception::badRequest(NX_FMT("`%1` must be a number.", nameForError()));
    }
    else if (typeName == "integer")
    {
        if (!value.isDouble())
            throw Exception::badRequest(NX_FMT("`%1` must be an integer.", nameForError()));
    }
    else if (typeName == "boolean")
    {
        if (!value.isBool())
            throw Exception::badRequest(NX_FMT("`%1` must be a boolean.", nameForError()));
    }
}

void validateParameters(
    const QJsonObject& schema,
    const rapidjson::Value& value,
    const QString& name,
    std::vector<QString>* unused);

void validateObjectParameters(
    const QJsonObject& schema,
    const rapidjson::Value& object,
    const QString& name,
    std::vector<QString>* unused)
{
    const auto properties = optObject(schema, "properties", name);
    auto required = optArray(schema, "required", name);
    for (auto field = object.MemberBegin(); field != object.MemberEnd(); ++field)
    {
        const auto fieldNameQ =
            QString::fromUtf8(field->name.GetString(), (int) field->name.GetStringLength());
        const auto nestedName{
            name.isEmpty() ? escapeName(fieldNameQ) : name + '.' + escapeName(fieldNameQ)};
        if (const auto fieldSchema = properties.find(fieldNameQ); fieldSchema != properties.end())
        {
            for (auto requiredValue = required.begin(); requiredValue != required.end();
                 ++requiredValue)
            {
                const auto requiredName{requiredValue->toString()};
                if (NX_ASSERT(!requiredName.isEmpty(), nestedName) && requiredName == fieldNameQ)
                {
                    required.erase(requiredValue);
                    break;
                }
            }

            const auto fieldObject{asObject(*fieldSchema, nestedName)};
            if (optBool(fieldObject, "readOnly", nestedName))
                unused->push_back(nestedName);
            else
                validateParameters(fieldObject, field->value, nestedName, unused);
        }
        // TODO: Input node should not be `unused` if it is allowed by `additionalProperties`.
        else if (!properties.empty())
        {
            unused->push_back(nestedName);
        }
    }
    for (const auto& requiredValue: required)
    {
        const auto requiredName{requiredValue.toString()};
        if (NX_ASSERT(!requiredName.isEmpty(), requiredName))
        {
            throw Exception::missingParameter(
                name.isEmpty() ? requiredName : unescapeName(name) + '.' + requiredName);
        }
    }
}

void validateParameters(
    const QJsonObject& schema,
    const rapidjson::Value& value,
    const QString& name,
    std::vector<QString>* unused)
{
    if (schema.isEmpty())
        return;

    QJsonObject additionalSchema;
    QString typeName;
    if (const auto type = schema.find("type"); type == schema.end())
    {
        additionalSchema = optObject(schema, "additionalProperties", name);
        if (additionalSchema.isEmpty())
            return;
    }
    else
    {
        typeName = type->toString();
    }

    const auto nameForError =
        [&name] { return name.isEmpty() ? QString("Request body") : unescapeName(name); };
    if (value.IsNull())
        throw Exception::badRequest(NX_FMT("`%1` must not be null", nameForError()));

    if (typeName.isEmpty() || typeName == "object")
    {
        if (!value.IsObject())
            throw Exception::badRequest(NX_FMT("`%1` must be an object", nameForError()));

        if (!typeName.isEmpty())
            additionalSchema = optObject(schema, "additionalProperties", name);
        if (!additionalSchema.isEmpty())
        {
            for (auto field = value.MemberBegin(); field != value.MemberEnd(); ++field)
            {
                const auto fieldName = escapeName(QString::fromUtf8(
                    field->name.GetString(), (int) field->name.GetStringLength()));
                const auto nestedName{name.isEmpty() ? fieldName : name + '.' + fieldName};
                validateParameters(additionalSchema, field->value, nestedName, unused);
            }
            return;
        }

        validateObjectParameters(schema, value, name, unused);
    }
    else if (typeName == "array")
    {
        if (!value.IsArray())
            throw Exception::badRequest(NX_FMT("`%1` must be an array", nameForError()));

        const auto items = getObject(schema, "items");
        if (!items.isEmpty())
        {
            for (size_t i = 0, n = value.Size(); i < n; ++i)
                validateParameters(items, value[i], name + '[' + nx::toString(i) + ']', unused);
        }
    }
    else if (typeName == "string")
    {
        if (!value.IsString())
            throw Exception::badRequest(NX_FMT("`%1` must be a string", nameForError()));
        // TODO: Check for the format of the actual values here.
    }
    else if (typeName == "number")
    {
        if (!value.IsNumber())
            throw Exception::badRequest(NX_FMT("`%1` must be a number", nameForError()));
    }
    else if (typeName == "integer")
    {
        if (!value.IsInt())
            throw Exception::badRequest(NX_FMT("`%1` must be an integer", nameForError()));
    }
    else if (typeName == "boolean")
    {
        if (!value.IsBool())
            throw Exception::badRequest(NX_FMT("`%1` must be a boolean", nameForError()));
    }
}

bool isRequired(const QJsonObject& object)
{
    const auto required = object.find("required");
    if (required == object.end())
        return false;
    const auto& requiredValue = required.value();
    return requiredValue.isBool() && requiredValue.toBool();
}

void validateParameterAndRemoveFromContent(const QJsonObject& schema, QJsonObject* content)
{
    const auto name = schema.find("name");
    if (!NX_ASSERT(name != schema.end(), QJson::serialized(schema)))
        return;
    const auto& nameString = name->toString();
    if (!NX_ASSERT(!nameString.isEmpty(), QJson::serialized(schema)))
        return;
    if (const auto value = content->take(nameString); value.isUndefined())
    {
        if (isRequired(schema))
        {
            NX_DEBUG(typeid(OpenApiSchema), "Content %1 missing '%2', required by %3",
                QJson::serialized(*content), nameString, QJson::serialized(schema));
            throw Exception::missingParameter(nameString);
        }
    }
    // TODO: Check for the type and format of exact values here.
}

// TODO: #skolesnik Write unit tests.
void removePathFromValue(const QString& path, QJsonValue* value)
{
    const auto separatorIndex = path.indexOf('.');

    if (separatorIndex == -1)
    {
        if (!path.endsWith(']'))
        {
            if (!NX_ASSERT(value->isObject()))
                return;

            QJsonObject obj = value->toObject();
            const auto originalPath = unescapeName(path);
            NX_ASSERT(obj.contains(originalPath));

            obj.remove(originalPath);
            *value = std::move(obj);
            return;
        }

        if (!NX_ASSERT(value->isArray()))
            return;

        // TODO:
        //   Remove from array (although it is a very unlike scenario).
        //   Also if an array have several objects for removal, it should be done in one go,
        //   since it will be very expensive (Qt's json does not allow for in-place modification).

        return;
    }

    const QString head = path.first(separatorIndex);
    const QString tail = path.sliced(separatorIndex + 1);

    if (!NX_ASSERT(value->isObject()))
        return;

    QJsonObject obj = value->toObject();


    if (head.endsWith(']'))
    {
        const auto bracketIndex = head.indexOf('[');
        if (!NX_ASSERT(bracketIndex != -1))
            return;

        const QString arrayName = unescapeName(head.first(bracketIndex));
        const auto arrayIndex =
            head.sliced(bracketIndex + 1, head.length() - bracketIndex - 2).toInt();

        const auto arrayFound = obj.find(arrayName);
        if (!NX_ASSERT(arrayFound != obj.end()) ||
            !NX_ASSERT(arrayFound->isArray()))
        {
            return;
        }

        QJsonArray arr = arrayFound->toArray();
        if (!NX_ASSERT(arrayIndex < arr.size()))
            return;

        QJsonValue childValue = arr.at(arrayIndex);
        removePathFromValue(tail, &childValue);
        arr.replace(arrayIndex, childValue);
        obj[arrayName] = std::move(arr);
        *value = std::move(obj);

        return;
    }
    const auto originalHead = unescapeName(head);
    QJsonValue childValue = obj.value(originalHead);
    removePathFromValue(tail, &childValue);
    obj.insert(originalHead, childValue);
    *value = std::move(obj);
}

void removePathFromValue(const std::string& path, rapidjson::Value* value)
{
    const auto separatorIndex = path.find_first_of('.');
    if (separatorIndex == std::string::npos)
    {
        if (!path.ends_with(']'))
        {
            if (NX_ASSERT(value->IsObject()))
            {
                if (auto it = value->FindMember(unescapeName(path));
                    NX_ASSERT(it != value->MemberEnd()))
                {
                    value->RemoveMember(it);
                }
            }
            return;
        }

        NX_ASSERT(value->IsArray());
        return;
    }

    const auto head{path.substr(0, separatorIndex)};
    const auto tail{path.substr(separatorIndex + 1)};
    if (head.ends_with(']'))
    {
        const auto bracketIndex = head.find_first_of('[');
        if (!NX_ASSERT(bracketIndex != std::string::npos))
            return;

        const auto arrayName{unescapeName(head.substr(0, bracketIndex))};
        const auto arrayIndex =
            std::stoul(head.substr(bracketIndex + 1, head.length() - bracketIndex - 2));

        const auto array = value->FindMember(arrayName);
        if (NX_ASSERT(array != value->MemberEnd())
            && NX_ASSERT(array->value.IsArray())
            && NX_ASSERT(arrayIndex < array->value.Size()))
        {
            removePathFromValue(tail, &array->value[arrayIndex]);
        }
        return;
    }

    if (NX_ASSERT(value->IsObject()))
    {
        if (auto it = value->FindMember(unescapeName(head)); it != value->MemberEnd())
            removePathFromValue(tail, &it->value);
    }
}

void removeUnused(const std::vector<QString>& unused, QJsonValue* from)
{
    // TODO: sort.
    //  - arr[N] must go before arr[0]
    for (const QString& path: unused)
        removePathFromValue(path, from);
}

} // anonymous namespace

void OpenApiSchema::postprocessResponse(
    const QJsonObject& path,
    const QJsonObject& method,
    const Params& params,
    QJsonValue* response) const
{
    if (path.isEmpty() || method.isEmpty())
        return;

    const auto content =
        optObject(optObject(optObject(method, "responses"), "default"), "content");
    if (content.isEmpty())
        return;

    auto orderByValues = params.values("_orderBy");
    orderByValues.removeAll(QString());
    Postprocessor{{orderByValues}}(
        response, getObject(getObject(content, "application/json"), "schema"));
}

void OpenApiSchema::postprocessResponse(
    const QJsonObject& path, const QJsonObject& method, rapidjson::Value* response) const
{
    if (path.isEmpty() || method.isEmpty())
        return;

    const auto content =
        optObject(optObject(optObject(method, "responses"), "default"), "content");
    if (content.isEmpty())
        return;

    PostprocessorV4{}(response, getObject(getObject(content, "application/json"), "schema"));
}

void OpenApiSchema::validateOrThrow(const QJsonObject& path,
    const QJsonObject& method,
    Request* request,
    http::HttpHeaders* headers)
{
    if (path.isEmpty() || method.isEmpty())
        return;

    validateParameters(path, *request);
    validateParameters(method, *request);

    if (request->method().isMessageBodyAllowed())
    {
        (void)[&]
        {
            const auto body = optObject(method, "requestBody");
            if (body.isEmpty())
                return;

            std::vector<QString> unused{};
            QJsonValue content;
            rapidjson::Value* params = nullptr;
            if (request->jsonRpcContext())
            {
                if (!request->jsonRpcContext()->request.params)
                {
                    if (isRequired(body))
                        throw Exception::badRequest("Missing request content");
                    return;
                }

                params = &request->jsonRpcContext()->request.params.value();
                json::validateParameters(
                    getObject(getObject(getObject(body, "content"), "application/json"), "schema"),
                    *params, /*name*/ {}, &unused);
            }
            else
            {
                content = request->content()
                    ? request->content()->parseOrThrow()
                    : QJsonValue{QJsonValue::Undefined};
                if (content.isUndefined())
                {
                    if (isRequired(body))
                        throw Exception::badRequest("Missing request content");
                    return;
                }

                json::validateParameters(
                    getObject(getObject(getObject(body, "content"), "application/json"), "schema"),
                    content, /*name*/ {}, &unused);
            }

            // Ad hoc to remove url params from `unused` since they are allowed to be passed
            // in the body for historical reasons.
            for (const auto param: optArray(method, "parameters"))
            {
                const auto paramObject = asObject(param);
                const auto name = getString(paramObject, "name");
                unused.erase(std::remove(unused.begin(), unused.end(), name), unused.end());
            }

            if (unused.empty())
                return;

            if (headers)
            {
                for (const QString& u: unused)
                {
                    headers->emplace("Warning",
                        nx::format("199 - \"Unused parameter: '%1'\"", unescapeName(u))
                            .toStdString());
                }
            }

            if (const auto v = request->apiVersion(); NX_ASSERT(v) && *v > 2)
            {
                const bool isStrict = request->params().value("_strict", "false") != "false";
                if (isStrict)
                {
                    throw Exception::badRequest(nx::format(
                        "Parameter '%1' is not allowed by the Schema.", unescapeName(unused[0])));
                }
            }

            if (params)
            {
                for (const QString& path: unused)
                    removePathFromValue(path.toStdString(), params);
                request->resetParamsCache();
            }
            else
            {
                removeUnused(unused, &content);
                request->updateContent(content);
            }
        }();
    }

    auto orderByValues = request->params().values("_orderBy");
    orderByValues.removeAll(QString());
    if (orderByValues.isEmpty())
        return;

    if (optObject(optObject(optObject(method, "responses"), "default"), "content").isEmpty())
        throw Exception::invalidParameter("_orderBy", nx::toString(orderByValues));

    for (const auto param: optArray(method, "parameters"))
    {
        const auto paramObject = asObject(param);
        if (getString(paramObject, "name") != "_orderBy")
            continue;

        const auto schemaObject = getObject(paramObject, "schema");
        const bool isArray = schemaObject.value("type") == "array";
        if (!isArray && orderByValues.size() > 1)
            throw Exception::invalidParameter("_orderBy", nx::toString(orderByValues));
        const auto expectedEnumValues = isArray
            ? getArray(getObject(schemaObject, "items"), "enum")
            : getArray(schemaObject, "enum");
        for (const auto& value: orderByValues)
        {
            if (std::find_if(expectedEnumValues.begin(),
                    expectedEnumValues.end(),
                    [value](auto item) { return asString(item) == value; })
                == expectedEnumValues.end())
            {
                throw Exception::invalidParameter("_orderBy", nx::toString(value));
            }
        }
        return;
    }
    throw Exception::invalidParameter("_orderBy", nx::toString(orderByValues));
}

QJsonObject OpenApiSchema::paths() const
{
    return json::getObject(m_schema, "paths");
}

std::pair<QJsonObject, QJsonObject> OpenApiSchema::getJsonPathAndMethod(
    const QString& method, const QString& decodedPath)
{
    std::pair<QJsonObject, QJsonObject> result;
    if (!NX_ASSERT(!decodedPath.isEmpty()))
        return result;
    const auto paths = json::getObject(m_schema, "paths");
    if (!NX_ASSERT(!paths.isEmpty()))
        return result;
    const auto path = paths.find(decodedPath);
    if (path == paths.end())
        throw Exception::notFound(NX_FMT("Unknown scheme path %1", decodedPath));
    auto& [pathObject, methodObject] = result;
    pathObject = path->toObject();
    if (!NX_ASSERT(!pathObject.isEmpty()))
        return result;
    const auto methodIt = pathObject.find(method);
    if (methodIt == pathObject.end())
    {
        throw Exception::internalServerError(
            NX_FMT("Unknown scheme method %1 for %2", method, decodedPath));
    }
    methodObject = methodIt->toObject();
    NX_ASSERT(!methodObject.isEmpty());
    return result;
}

bool OpenApiSchema::denormalize(QJsonArray* data, int depthLimit)
{
    QJsonArray denormalized;
    bool isDenormalized = false;
    for (const auto& item: *data)
    {
        if (item.isObject())
        {
            QJsonObject itemObject = item.toObject();
            if (denormalize(&itemObject, depthLimit))
                isDenormalized = true;
            denormalized.append(itemObject);
        }
        else
        {
            denormalized.append(item);
        }
    }
    if (isDenormalized)
        data->swap(denormalized);
    return isDenormalized;
}

bool OpenApiSchema::denormalize(QJsonObject* data, int depthLimit)
{
    if (const auto ref = data->find("$ref"); ref != data->end())
    {
        *data = getRef(ref.value());
        if (--depthLimit >= 0)
            denormalize(data, depthLimit);
        return true;
    }
    bool isDenormalized = false;
    QJsonObject denormalized;
    for (auto it = data->begin(); it != data->end(); ++it)
    {
        const auto& key = it.key();
        auto value = it.value();
        if (key == "allOf")
        {
            fillAllOf(value.toArray(), &denormalized);
            isDenormalized = true;
            continue;
        }
        if (value.isArray())
        {
            QJsonArray field = value.toArray();
            if (denormalize(&field, depthLimit))
                isDenormalized = true;
            denormalized[key] = field;
            continue;
        }
        if (value.isObject())
        {
            QJsonObject field = value.toObject();
            if (denormalize(&field, depthLimit))
                isDenormalized = true;
            denormalized[key] = field;
            continue;
        }
        denormalized[key] = value;
    }
    if (isDenormalized)
        data->swap(denormalized);
    return isDenormalized;
}

QJsonObject OpenApiSchema::getRef(const QJsonValue& value)
{
    if (!NX_ASSERT(value.isString()))
        return QJsonObject();
    QString path = value.toString();
    if (!NX_ASSERT(path.size() > 3 && path[0] == '#' && path[1] == '/'))
        return QJsonObject();
    path = path.right(path.length() - 2);
    QJsonObject ref = m_schema;
    while (!path.isEmpty())
    {
        const int pos = path.indexOf('/');
        const QString item = path.left(pos);
        path = (pos == -1) ? QString() : path.right(path.length() - pos - 1);
        const auto nested = ref.find(item);
        if (!NX_ASSERT(nested != ref.end()))
            return QJsonObject();
        ref = nested->toObject();
        if (!NX_ASSERT(!ref.isEmpty()))
            return ref;
    }
    return ref;
}

void OpenApiSchema::fillAllOf(const QJsonValue& value, QJsonObject* data)
{
    if (!NX_ASSERT(value.isArray()))
        return;
    for (const auto& item: value.toArray())
    {
        if (!NX_ASSERT(item.isObject()))
            return;
        auto itemObject = item.toObject();
        denormalize(&itemObject);
        merge(data, std::move(itemObject));
    }
}

void OpenApiSchema::validateParameters(const QJsonObject& pathOrMethod, const Request& request)
{
    const auto params = pathOrMethod.find("parameters");
    if (params == pathOrMethod.end())
        return;
    const auto& paramsValue = params.value();
    if (!NX_ASSERT(paramsValue.isArray()))
        return;

    // TODO: Use nx_reflect here.
    auto content = request.params().isEmpty() ? QJsonObject() : request.params().toJson();
    for (const auto& param: paramsValue.toArray())
    {
        auto paramObject = param.toObject();
        if (NX_ASSERT(!paramObject.isEmpty()))
            validateParameterAndRemoveFromContent(paramObject, &content);
    }
    for (auto it = content.constBegin(); it != content.constEnd(); ++it)
    {
        if (it.key().startsWith(QChar('_')))
            throw Exception::badRequest(NX_FMT("Unknown parameter `%1`.", it.key()));
    }
}

SchemaWordSearchResult OpenApiSchema::searchForWord(const QString& word) const
{
    SchemaWordSearchResult locations;
    searchForWordInJson(paths(), word, QString(), &locations);
    return locations;
}

void OpenApiSchema::searchForWordInJson(const QJsonValue& value,
    const QString& word,
    const QString& path,
    SchemaWordSearchResult* locations)
{
    if (value.isObject())
    {
        for (const QJsonObject obj = value.toObject(); const QString& key : obj.keys())
        {
            QString newPath = path.isEmpty() ? key : path + "." + key;
            if (key.contains(word, Qt::CaseInsensitive))
                locations->emplace_back(newPath, key);

            searchForWordInJson(obj.value(key), word, newPath, locations);
        }
    }
    else if (value.isArray())
    {
        const QJsonArray array = value.toArray();
        for (int i = 0; i < array.size(); ++i)
        {
            QString newPath = path + "[" + QString::number(i) + "]";
            searchForWordInJson(array.at(i), word, newPath, locations);
        }
    }
    else if (value.isString())
    {
        if (value.toString().contains(word, Qt::CaseInsensitive))
            locations->emplace_back(path, value.toString());
    }
}

std::shared_ptr<OpenApiSchema> OpenApiSchema::load(const QString& location)
{
    QFile file(location);
    if (!NX_ASSERT(file.open(QFile::ReadOnly), location))
        return nullptr;

    const auto content = file.readAll();
    if (!NX_ASSERT(!content.isEmpty()))
        return nullptr;

    if (!NX_ASSERT((int) file.size() == content.size(),
        "Expected %1, read %2", file.size(), content.size()))
    {
        return nullptr;
    }

    QnJsonContext ctx;
    QJsonObject schema;
    if (!NX_ASSERT(QJson::deserialize(&ctx, content, &schema), ctx.getFailedKeyValue()))
        return nullptr;

    if (!NX_ASSERT(!schema.empty()))
        return nullptr;

    return std::make_shared<OpenApiSchema>(std::move(schema));
}

OpenApiSchemas::OpenApiSchemas(std::vector<std::shared_ptr<OpenApiSchema>> schemas):
    m_schemas(std::move(schemas))
{
}

void OpenApiSchemas::validateOrThrow(Request* request, http::HttpHeaders* headers)
{
    const auto decodedPath = request->decodedPath();
    if (!NX_ASSERT(!decodedPath.isEmpty()))
        return;

    auto [schema, path, method] = findSchema(
        decodedPath,
        request->method(),
        /*throwIfNotFound*/ true);
    if (schema)
        schema->validateOrThrow(path, method, request, headers);
}

void OpenApiSchemas::postprocessResponse(
    const nx::network::http::Method& method,
    const Params& params,
    const QString& decodedPath,
    QJsonValue* response) const
{
    if (!NX_ASSERT(!decodedPath.isEmpty()))
        return;

    auto [schema, path, method_] = findSchema(decodedPath, method);
    if (schema)
        schema->postprocessResponse(path, method_, params, response);
}

void OpenApiSchemas::postprocessResponse(
    const nx::network::http::Method& method,
    const QString& decodedPath,
    rapidjson::Value* response) const
{
    if (!NX_ASSERT(!decodedPath.isEmpty()))
        return;

    auto [schema, path, method_] = findSchema(decodedPath, method);
    if (schema)
        schema->postprocessResponse(path, method_, response);
}

std::tuple<std::shared_ptr<OpenApiSchema>, QJsonObject, QJsonObject> OpenApiSchemas::findSchema(
    const QString& requestPath, const nx::network::http::Method& method_, bool throwIfNotFound) const
{
    const QString schemaMethod = QString::fromStdString(method_.toString()).toLower();
    for (std::size_t version = 0; version < m_schemas.size(); ++version)
    {
        auto schema = m_schemas[version];
        const auto paths = schema->paths();
        if (!NX_ASSERT(!paths.isEmpty()))
        {
            throwIfNotFound = false;
            continue;
        }

        const auto path = paths.find(requestPath);
        if (path == paths.end())
            continue;

        auto pathObject = path->toObject();
        if (!NX_ASSERT(!pathObject.isEmpty()))
        {
            throwIfNotFound = false;
            continue;
        }

        const auto method = pathObject.find(schemaMethod);
        if (method == pathObject.end())
        {
            throw Exception::notImplemented(
                NX_FMT("Unknown OpenAPI schema method %1 for %2", method_, requestPath));
        }
        auto methodObject = method->toObject();
        NX_ASSERT(!methodObject.isEmpty());
        return {std::move(schema), std::move(pathObject), std::move(methodObject)};
    }

    if (throwIfNotFound)
    {
        throw Exception::notFound(
            NX_FMT("Unknown OpenAPI schema path %1", requestPath));
    }
    return {};
}

} // namespace nx::network::rest::json
