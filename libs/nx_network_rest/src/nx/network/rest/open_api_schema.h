// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>

#include <nx/network/http/http_types.h>

#include "request.h"

namespace nx::network::rest::json {

// See the documentation here: https://swagger.io/docs/specification/
class NX_NETWORK_REST_API OpenApiSchema
{
public:
    OpenApiSchema(QJsonObject schema): m_schema(std::move(schema)) { denormalize(&m_schema); }
    void postprocessResponse(const Request& request, QJsonValue* response);
    void postprocessResponse(
        const QJsonObject& path,
        const QJsonObject& method,
        const Request& request,
        QJsonValue* response);

    void validateOrThrow(Request* request, nx::network::http::HttpHeaders* = nullptr);
    void validateOrThrow(const QJsonObject& path,
        const QJsonObject& method,
        Request* request,
        nx::network::http::HttpHeaders* = nullptr);

    QJsonObject paths() const;

    static std::shared_ptr<OpenApiSchema> load(const QString& location);

private:
    // TODO: #ekarpov Support unlimited denormalization on demand.
    bool denormalize(QJsonObject* data, int depthLimit = 5);
    bool denormalize(QJsonArray* data, int depthLimit);
    QJsonObject getRef(const QJsonValue& value);
    void fillAllOf(const QJsonValue& value, QJsonObject* data);
    void validateParameters(const QJsonObject& pathOrMethod, const Request& request);
    std::pair<QJsonObject, QJsonObject> getJsonPathAndMethod(const Request& request);

private:
    QJsonObject m_schema;
};

class NX_NETWORK_REST_API OpenApiSchemas
{
public:
    OpenApiSchemas(std::vector<std::shared_ptr<OpenApiSchema>> schemas);
    void validateOrThrow(Request* request, nx::network::http::HttpHeaders* = nullptr);
    void postprocessResponse(const Request& request, QJsonValue* response);

private:
    std::tuple<std::shared_ptr<OpenApiSchema>, QJsonObject, QJsonObject> findSchema(
        const QString& requestPath, const QString& requestMethod, bool throwIfNotFound = false);

private:
    std::vector<std::shared_ptr<OpenApiSchema>> m_schemas;
};

} // namespace nx::network::rest::json
