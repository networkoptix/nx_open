// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <rapidjson/document.h>

#include <QtCore/QJsonObject>

#include <nx/network/http/http_types.h>

namespace nx::network::rest { class Params; }
namespace nx::network::rest { struct Request; }

namespace nx::network::rest::json {

using SchemaWordSearchResult = std::vector<std::pair<QString, QString>>;

// See the documentation here: https://swagger.io/docs/specification/
class NX_NETWORK_REST_API OpenApiSchema
{
public:
    OpenApiSchema(QJsonObject schema): m_schema(std::move(schema)) { denormalize(&m_schema); }
    void postprocessResponse(
        const QJsonObject& path,
        const QJsonObject& method,
        const Params& params,
        QJsonValue* response) const;

    void postprocessResponse(
        const QJsonObject& path, const QJsonObject& method, rapidjson::Value* response) const;

    void validateOrThrow(const QJsonObject& path,
        const QJsonObject& method,
        Request* request,
        nx::network::http::HttpHeaders* = nullptr);

    QJsonObject paths() const;
    SchemaWordSearchResult searchForWord(const QString& word) const;

    static std::shared_ptr<OpenApiSchema> load(const QString& location);

private:
    // TODO: #ekarpov Support unlimited denormalization on demand.
    bool denormalize(QJsonObject* data, int depthLimit = 5);
    bool denormalize(QJsonArray* data, int depthLimit);
    QJsonObject getRef(const QJsonValue& value);
    void fillAllOf(const QJsonValue& value, QJsonObject* data);
    void validateParameters(const QJsonObject& pathOrMethod, const Request& request);
    std::pair<QJsonObject, QJsonObject> getJsonPathAndMethod(
        const QString& method, const QString& decodedPath);

    static void searchForWordInJson(const QJsonValue& value,
        const QString& word,
        const QString& path,
        SchemaWordSearchResult* locations);

private:
    QJsonObject m_schema;
};

class NX_NETWORK_REST_API OpenApiSchemas
{
public:
    OpenApiSchemas(std::vector<std::shared_ptr<OpenApiSchema>> schemas);
    void validateOrThrow(Request* request, nx::network::http::HttpHeaders* = nullptr);
    void postprocessResponse(
        const nx::network::http::Method& method,
        const Params& params,
        const QString& decodedPath,
        QJsonValue* response) const;

    void postprocessResponse(
        const nx::network::http::Method& method,
        const QString& decodedPath,
        rapidjson::Value* response) const;

private:
    std::tuple<std::shared_ptr<OpenApiSchema>, QJsonObject, QJsonObject> findSchema(
        const QString& requestPath,
        const nx::network::http::Method& method,
        bool throwIfNotFound = false) const;

private:
    std::vector<std::shared_ptr<OpenApiSchema>> m_schemas;
};

} // namespace nx::network::rest::json
