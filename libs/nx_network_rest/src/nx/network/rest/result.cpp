// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "result.h"

#include <nx/utils/switch.h>

namespace nx::network::rest {

QString Result::errorToString(Result::Error value)
{
    switch (value)
    {
        case Result::NoError: return "ok";
        case Result::MissingParameter: return "missingParameter";
        case Result::InvalidParameter: return "invalidParameter";
        case Result::CantProcessRequest: return "cantProcessRequest";
        case Result::Forbidden: return "forbidden";
        case Result::BadRequest: return "badRequest";
        case Result::InternalServerError: return "internalServerError";
        case Result::Conflict: return "conflict";
        case Result::NotAllowed: return "notAllowed";
        case Result::NotImplemented: return "notImplemented";
        case Result::NotFound: return "notFound";
        case Result::UnsupportedMediaType: return "unsupportedMediaType";
        case Result::ServiceUnavailable: return "serviceUnavailable";
        case Result::Unauthorized: return "unauthorized";
        case Result::SessionExpired: return "sessionExpired";
        case Result::SessionRequired: return "sessionRequired";
        case Result::SessionTruncated: return "sessionTruncated";
        case Result::Gone: return "gone";
    };

    return NX_FMT("Unknown_%1", static_cast<int>(value));
}

nx::network::http::StatusCode::Value Result::toHttpStatus(Error code)
{
    using namespace nx::network;
    switch (code)
    {
        case Error::NoError:
            return http::StatusCode::ok;

        case Error::InvalidParameter:
        case Error::MissingParameter:
        case Error::CantProcessRequest:
            return http::StatusCode::unprocessableEntity;

        case Error::Forbidden:
        case Error::SessionExpired:
        case Error::SessionRequired:
        case Error::SessionTruncated:
            return http::StatusCode::forbidden;

        case Error::Conflict:
            return http::StatusCode::conflict;

        case Error::NotAllowed:
            return http::StatusCode::notAllowed;

        case Error::NotImplemented:
            return http::StatusCode::notImplemented;

        case Error::NotFound:
            return http::StatusCode::notFound;

        case Error::UnsupportedMediaType:
            return http::StatusCode::unsupportedMediaType;

        case Error::ServiceUnavailable:
            return http::StatusCode::serviceUnavailable;

        case Error::Unauthorized:
            return http::StatusCode::unauthorized;

        case Error::InternalServerError:
            return http::StatusCode::internalServerError;

        case Error::Gone:
            return http::StatusCode::gone;

        default:
            return http::StatusCode::badRequest;
    }
}

Result::Error Result::errorFromHttpStatus(int status)
{
    using namespace nx::network::http;
    switch (status)
    {
        case StatusCode::ok:
            return Error::NoError;

        case StatusCode::unprocessableEntity:
            return Error::CantProcessRequest;

        case StatusCode::forbidden:
            return Error::Forbidden;

        case StatusCode::conflict:
            return Error::Conflict;

        case StatusCode::notAllowed:
            return Error::NotAllowed;

        case StatusCode::notImplemented:
            return Error::NotImplemented;

        case StatusCode::notFound:
            return Error::NotFound;

        case StatusCode::unsupportedMediaType:
            return Error::UnsupportedMediaType;

        case StatusCode::serviceUnavailable:
            return Error::ServiceUnavailable;

        case StatusCode::internalServerError:
            return Error::InternalServerError;

        case StatusCode::unauthorized:
            return Error::Unauthorized;

        default:
            return Error::BadRequest;
    }
}

Result::Result(Error error, QString errorString):
    error(error),
    errorString(std::move(errorString))
{
}

QString Result::invalidParameterTemplate()
{
    return tr("Invalid parameter `%1`: %2.", /*comment*/ "%1 is name, %2 is value.");
}

Result Result::missingParameter(const QString& name)
{
    return Result{MissingParameter, tr("Missing required parameter: %1.").arg(name)};
}

Result Result::cantProcessRequest(std::optional<QString> customMessage)
{
    return Result{CantProcessRequest, customMessage
        ? *customMessage
        : tr("Failed to process request.")};
}

Result Result::forbidden(std::optional<QString> customMessage)
{
    return Result{
        Forbidden,
        customMessage ? *customMessage : tr("Forbidden.", /*comment*/ "Generic HTTP response")};
}

Result Result::conflict(std::optional<QString> customMessage)
{
    return Result{Conflict,
        customMessage ? *customMessage : tr("Conflict.", /*comment*/ "Generic HTTP response")};
}

Result Result::badRequest(std::optional<QString> customMessage)
{
    return Result{
        BadRequest,
        customMessage ? *customMessage : tr("Bad request.",/*comment*/ "Generic HTTP response")};
}

Result Result::notAllowed(std::optional<QString> customMessage)
{
    return Result{NotAllowed,
        customMessage ? *customMessage : tr("Not allowed.", /*comment*/ "Generic HTTP response")};
}

Result Result::notImplemented(std::optional<QString> customMessage)
{
    return Result{NotImplemented,
        customMessage ? *customMessage : tr("Not implemented.",/*comment*/ "Generic HTTP response")};
}

Result Result::notFound(std::optional<QString> customMessage)
{
    return Result{
        NotFound,
        customMessage ? *customMessage : tr("Not found.",/*comment*/ "Generic HTTP response")};
}

Result Result::internalServerError(std::optional<QString> customMessage)
{
    return Result{InternalServerError, customMessage
        ? *customMessage
        : tr("Internal error.")};
}

Result Result::unsupportedMediaType(std::optional<QString> customMessage)
{
    return Result{UnsupportedMediaType, customMessage
        ? *customMessage
        : tr("Unsupported media type.")};
}

Result Result::serviceUnavailable(std::optional<QString> customMessage)
{
    return Result{ServiceUnavailable, customMessage ? *customMessage : tr("Service unavailable.")};
}

Result Result::unauthorized(std::optional<QString> customMessage)
{
    return Result{Unauthorized, customMessage ? *customMessage : tr("Unauthorized.")};
}

Result Result::sessionExpired(std::optional<QString> customMessage)
{
    return Result{SessionExpired, customMessage ? *customMessage : tr("Session expired.")};
}

Result Result::sessionRequired(std::optional<QString> customMessage)
{
    return Result{SessionRequired,
        customMessage ? *customMessage : tr("Session authorization required.")};
}

Result Result::sessionTruncated(std::optional<QString> customMessage)
{
    return Result{SessionTruncated,
        customMessage ? *customMessage : tr("Session is too old according to the Site config.")};
}

Result Result::gone(std::optional<QString> customMessage)
{
    return Result{Gone,
        customMessage ? *customMessage : tr("Resource no longer present on server.")};
}

void serialize(const Result::Error& value, QString* target)
{
    *target = QString::number(static_cast<int>(value));
}

bool deserialize(const QString& value, Result::Error* target)
{
    *target = static_cast<Result::Error>(value.toInt());
    return true;
}

void serialize(QnJsonContext* /*context*/, const Result& value, QJsonValue* outTarget)
{
    *outTarget = QJsonObject{
        {"error", QJsonValue(QString::number(static_cast<int>(value.error)))},
        {"errorId", QJsonValue(Result::errorToString(value.error))},
        {"errorString", QJsonValue(value.errorString)},
    };
}

bool deserialize(QnJsonContext* context, QnConversionWrapper<QJsonValue> value, Result* outTarget)
{
    if (!QJsonValue(value).isObject())
    {
        context->setFailedKeyValue({"", "Not an object"});
        return false;
    }

    const auto object = QJsonValue(value).toObject();

    const auto error = object["error"];
    if (error.isString())
    {
        bool isOk = false;
        outTarget->error = static_cast<Result::Error>(error.toString().toInt(&isOk));
        if (!isOk)
        {
            context->setFailedKeyValue({"error", "Not an integer"});
            return false;
        }
    }
    else if (error.isDouble())
    {
        outTarget->error = static_cast<Result::Error>(error.toInt());
    }
    else
    {
        context->setFailedKeyValue({"error", "Not an integer"});
        return false;
    }

    if (object.contains("errorString"))
    {
        const auto errorString = object["errorString"];
        if (!errorString.isString())
        {
            context->setFailedKeyValue({"errorString", "Not a string"});
            return false;
        }

        outTarget->errorString = errorString.toString();
    }
    else
    {
        outTarget->errorString = "";
    }

    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Result, (ubjson)(xml)(csv_record), Result_Fields)

JsonResult::JsonResult(const Result& result):
    Result(result)
{
}

JsonResult::JsonResult(Error error, QString errorString):
    Result(error, std::move(errorString))
{
}

void JsonResult::writeError(QByteArray* outBody, Error error, const QString& errorMessage)
{
    JsonResult jsonRestResult(Result(error, errorMessage));
    *outBody = QJson::serialized(jsonRestResult);
}

void serialize(QnJsonContext* context, const JsonResult& value, QJsonValue* outTarget)
{
    serialize(context, (const Result&) value, outTarget);
    if (!value.reply.isNull())
    {
        auto object = outTarget->toObject();
        object["reply"] = value.reply;
        *outTarget = object;
    }
}

bool deserialize(
    QnJsonContext* context, QnConversionWrapper<QJsonValue> value, JsonResult* outTarget)
{
    if (!deserialize(context, value, (Result*) outTarget))
        return false;

    const auto object = QJsonValue(value).toObject();
    if (object.contains("reply"))
        outTarget->reply = object.value("reply");
    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(JsonResult, (ubjson), JsonResult_Fields)

UbjsonResult::UbjsonResult(const Result& result):
    Result(result)
{
}

UbjsonResult::UbjsonResult(Error error, QString errorString):
    Result(error, std::move(errorString))
{
}

void serialize(QnJsonContext*, const UbjsonResult&, QJsonValue* outTarget)
{
    NX_ASSERT(false, "We should not serialize UbjsonResult to QJsonValue.");
    *outTarget = QJsonValue();
}

bool deserialize(QnJsonContext*, QnConversionWrapper<QJsonValue>, UbjsonResult*)
{
    NX_ASSERT(false, "We should not deserialize UbjsonResult from QJsonValue.");
    return false;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UbjsonResult, (ubjson), UbjsonResult_Fields)

} // namespace nx::network::rest

// Dummy methods to make the Fusion macro compile for json and ubjson at once.

static void serialize(const QJsonValue& /*value*/, QnUbjsonWriter<QByteArray>* /*stream*/)
{
    NX_ASSERT(false, "We should not serialize QJsonValue to UBJson.");
}

static bool deserialize(QnUbjsonReader<QByteArray>* /*stream*/, QJsonValue* /*target*/)
{
    NX_ASSERT(false, "We should not serialize QJsonValue to UBJson.");
    return true;
}
