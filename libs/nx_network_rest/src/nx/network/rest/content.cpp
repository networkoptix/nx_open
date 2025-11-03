// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "content.h"

#include <QtCore/QCoreApplication>

#include "exception.h"
#include "params.h"

namespace nx::network::rest {

namespace {

struct ApiErrorStrings
{
    Q_DECLARE_TR_FUNCTIONS(ApiErrorStrings);
};

} // namespace

// TODO: Fix or refactor. This function disregards possible content errors. For example,
//     invalid JSON content (parsing errors) will be silently ignored.
//     For details: VMS-41649
std::optional<QJsonValue> Content::parse() const
{
    try
    {
        return parseOrThrow();
    }
    catch (const rest::Exception& e)
    {
        NX_DEBUG(this, "Content parsing error: \"%1\"", e.what());
        return std::nullopt;
    }
}

QJsonValue Content::parseOrThrow() const
{
    if (!type.charset.empty() && type.charset != http::header::ContentType::kDefaultCharset)
        throw Exception::unsupportedMediaType(ApiErrorStrings::tr("Unsupported charset."));

    if (type.value == http::header::ContentType::kForm.value)
        return Params::fromUrlQuery(QUrlQuery(body)).toJson();

    if (type.value == http::header::ContentType::kJson.value)
    {
        QJsonValue value;
        if (!QJsonDetail::deserialize_json(body, &value))
            throw Exception::badRequest(ApiErrorStrings::tr("Invalid JSON content."));

        return value;
    }

    // TODO: Other content types should go there when supported.
    // TODO: `Exception::unsupportedContentType`
    throw Exception::unsupportedMediaType(ApiErrorStrings::tr("Unsupported content type."));
}

} // namespace nx::network::rest
