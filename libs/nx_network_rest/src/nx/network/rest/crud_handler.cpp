// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "crud_handler.h"

#include "open_api_schema.h"

namespace nx::network::rest::detail {

static auto popFront(QString hierarchicalName)
{
    const int pos = hierarchicalName.indexOf('.');
    return std::make_pair(hierarchicalName.left(pos),
        (pos == -1) ? QString() : hierarchicalName.right(hierarchicalName.length() - pos - 1));
}

static void fillFilter(QString path, QString value, std::vector<nx::reflect::Filter>* fields)
{
    auto [pathItem, subpath] = popFront(std::move(path));
    if (pathItem.isEmpty())
        throw Exception::badRequest("Empty name of parameter");

    auto name = pathItem.toStdString();
    auto nested = nx::utils::find_if(*fields, [&name](const auto& f) { return f.name == name; });
    if (!nested)
    {
        // TODO: Replace with `nested = &fields->emplace_back(std::move(name))` when newer clang
        // will be used.
        nested = (fields->push_back({std::move(name)}), &fields->back());
    }
    if (!subpath.isEmpty())
        fillFilter(subpath, value, &nested->fields);
    else
        nested->values.emplace_back(value.toStdString());
}

static void fillOrder(QString path, std::vector<nx::reflect::ArrayOrder>* fields)
{
    auto [pathItem, subpath] = popFront(std::move(path));
    if (pathItem.isEmpty())
        throw Exception::invalidParameter("_orderBy", "Empty name of a field");

    if (pathItem.endsWith("[]"))
        pathItem = pathItem.left(pathItem.size() - 2);

    if (pathItem == "*")
    {
        if (subpath.isEmpty())
            throw Exception::invalidParameter("_orderBy", "Map must have at least one field");
        fillOrder(subpath, fields);
        return;
    }

    nx::reflect::ArrayOrder* nested = nullptr;
    if (pathItem.startsWith('#'))
    {
        bool ok = false;
        const auto index = pathItem.right(pathItem.size() - 1).toInt(&ok);
        if (!ok)
            throw Exception::invalidParameter("_orderBy", NX_FMT("Invalid oneOf %1", pathItem));
        nested = nx::utils::find_if(
            *fields, [index](const auto& f) { return f.variantIndex == index; });
        if (!nested)
        {
            // TODO: Replace with
            // `nested = &fields->emplace_back(nx::reflect::ArrayOrder{.variantIndex = index})`
            // when newer clang will be used.
            nested = (fields->push_back(nx::reflect::ArrayOrder{.variantIndex = index}), &fields->back());
        }
    }
    else
    {
        auto name = pathItem.toStdString();
        nested = nx::utils::find_if(*fields, [&name](const auto& f) { return f.name == name; });
        if (!nested)
        {
            // TODO: Replace with `nested = &fields->emplace_back(std::move(name))` when newer
            // clang will be used.
            nested = (fields->push_back({std::move(name)}), &fields->back());
        }
    }
    if (!subpath.isEmpty())
        fillOrder(subpath, &nested->fields);
}

static void filterAny(const utils::DotNotationString& items, rapidjson::Value* result);

static void filterArray(const utils::DotNotationString& items, rapidjson::Value* result)
{
    for (auto it = result->Begin(); it != result->End(); ++it)
        filterAny(items, &(*it));
}

static void filterObject(const utils::DotNotationString& items, rapidjson::Value* result)
{
    auto wildcardFilter = items.find("*");
    const bool wildcard = wildcardFilter != items.end();

    auto field = result->MemberBegin();
    while (field != result->MemberEnd())
    {
        auto it = items.find(field->name.GetString());
        auto& filter = it != items.end() ? it : wildcardFilter;
        if (filter != items.end())
        {
            if (const auto& nested = filter.value(); !nested.empty())
            {
                if (wildcard)
                {
                    utils::DotNotationString merged;
                    merged = wildcardFilter.value();
                    merged.add(nested);
                    filterAny(merged, &field->value);
                }
                else
                {
                    filterAny(nested, &field->value);
                }
            }
            if (field->value.IsObject() && field->value.ObjectEmpty())
                field = result->EraseMember(field);
            else
                ++field;
        }
        else
        {
            field = result->EraseMember(field);
        }
    }
}

static void filterAny(const utils::DotNotationString& items, rapidjson::Value* value)
{
    if (value->IsArray())
        return filterArray(items, value);
    if (value->IsObject())
        return filterObject(items, value);
    if (!items.empty())
        throw Exception::badRequest(NX_FMT("Unknown _with item(s): %1", items.keys().join(", ")));
}

nx::reflect::Filter filterFromParams(const Params& params)
{
    nx::reflect::Filter filter;
    for (auto [k, v]: params.keyValueRange())
    {
        if (k.isEmpty())
            throw Exception::badRequest("Empty name of parameter");
        if (!k.startsWith(QChar('_')))
            fillFilter(k, v, &filter.fields);
    }
    return filter;
}

nx::reflect::ArrayOrder orderFromParams(const Params& params)
{
    nx::reflect::ArrayOrder order;
    auto orderByValues = params.values("_orderBy");
    orderByValues.removeAll(QString());
    for (auto v: orderByValues)
        fillOrder(v, &order.fields);
    return order;
}

void setResponseContent(Response* response,
    QJsonValue value,
    Qn::SerializationFormat format,
    const json::OpenApiSchemas* schemas,
    const Request& request)
{
    if (NX_ASSERT(schemas))
    {
        schemas->postprocessResponse(
            request.method(), request.params(), request.decodedPath(), &value);
    }
    response->content = {Qn::serializationFormatToHttpContentType(format), QByteArray{}};
    if (request.jsonRpcContext())
        response->contentBodyJson = std::move(value);
    else
        response->content->body = json::serialized(value, format);
}

void setResponseContent(Response* response,
    rapidjson::Document value,
    Qn::SerializationFormat format,
    const json::OpenApiSchemas* schemas,
    const Request& request)
{
    if (NX_ASSERT(schemas))
        schemas->postprocessResponse(request.method(), request.decodedPath(), &value);
    response->content = {Qn::serializationFormatToHttpContentType(format), QByteArray{}};
    if (request.jsonRpcContext())
        response->contentBodyJson = std::move(value);
    else
        response->content->body = json::serialized(value, format);
}

} // namespace nx::network::rest::detail
