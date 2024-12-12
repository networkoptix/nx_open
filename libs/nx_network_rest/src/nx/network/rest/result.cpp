// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "result.h"

#include <nx/utils/switch.h>

namespace nx::network::rest {

nx::network::http::StatusCode::Value Result::toHttpStatus(ErrorId code)
{
    using namespace nx::network;
    switch (code)
    {
        case ErrorId::ok:
            return http::StatusCode::ok;

        case ErrorId::invalidParameter:
        case ErrorId::missingParameter:
        case ErrorId::cantProcessRequest:
            return http::StatusCode::unprocessableEntity;

        case ErrorId::forbidden:
        case ErrorId::sessionExpired:
        case ErrorId::sessionRequired:
        case ErrorId::sessionTruncated:
        case ErrorId::serviceUnauthorized:
            return http::StatusCode::forbidden;

        case ErrorId::conflict:
            return http::StatusCode::conflict;

        case ErrorId::notAllowed:
            return http::StatusCode::notAllowed;

        case ErrorId::notImplemented:
            return http::StatusCode::notImplemented;

        case ErrorId::notFound:
            return http::StatusCode::notFound;

        case ErrorId::unsupportedMediaType:
            return http::StatusCode::unsupportedMediaType;

        case ErrorId::serviceUnavailable:
            return http::StatusCode::serviceUnavailable;

        case ErrorId::unauthorized:
            return http::StatusCode::unauthorized;

        case ErrorId::internalServerError:
            return http::StatusCode::internalServerError;

        case ErrorId::gone:
            return http::StatusCode::gone;

        default:
            return http::StatusCode::badRequest;
    }
}

ErrorId Result::errorFromHttpStatus(int status)
{
    using namespace nx::network::http;
    switch (status)
    {
        case StatusCode::ok:
            return ErrorId::ok;

        case StatusCode::unprocessableEntity:
            return ErrorId::cantProcessRequest;

        case StatusCode::forbidden:
            return ErrorId::forbidden;

        case StatusCode::conflict:
            return ErrorId::conflict;

        case StatusCode::notAllowed:
            return ErrorId::notAllowed;

        case StatusCode::notImplemented:
            return ErrorId::notImplemented;

        case StatusCode::notFound:
            return ErrorId::notFound;

        case StatusCode::unsupportedMediaType:
            return ErrorId::unsupportedMediaType;

        case StatusCode::serviceUnavailable:
            return ErrorId::serviceUnavailable;

        case StatusCode::internalServerError:
            return ErrorId::internalServerError;

        case StatusCode::unauthorized:
            return ErrorId::unauthorized;

        default:
            return ErrorId::badRequest;
    }
}

Result::Result(ErrorId error, QString errorString):
    errorId(error),
    errorString(std::move(errorString))
{
}

QString Result::invalidParameterTemplate()
{
    return tr("Invalid parameter `%1`: %2", /*comment*/ "%1 is name, %2 is value.");
}

Result Result::missingParameter(const QString& name)
{
    return Result{ErrorId::missingParameter, tr("Missing required parameter: %1.").arg(name)};
}

Result Result::cantProcessRequest(std::optional<QString> customMessage)
{
    return Result{ErrorId::cantProcessRequest, customMessage
        ? *customMessage
        : tr("Failed to process request.")};
}

Result Result::forbidden(std::optional<QString> customMessage)
{
    return Result{
        ErrorId::forbidden,
        customMessage ? *customMessage : tr("Forbidden.", /*comment*/ "Generic HTTP response")};
}

Result Result::conflict(std::optional<QString> customMessage)
{
    return Result{ErrorId::conflict,
        customMessage ? *customMessage : tr("Conflict.", /*comment*/ "Generic HTTP response")};
}

Result Result::badRequest(std::optional<QString> customMessage)
{
    return Result{
        ErrorId::badRequest,
        customMessage ? *customMessage : tr("Bad request.",/*comment*/ "Generic HTTP response")};
}

Result Result::notAllowed(std::optional<QString> customMessage)
{
    return Result{ErrorId::notAllowed,
        customMessage ? *customMessage : tr("Not allowed.", /*comment*/ "Generic HTTP response")};
}

Result Result::notImplemented(std::optional<QString> customMessage)
{
    return Result{ErrorId::notImplemented,
        customMessage ? *customMessage : tr("Not implemented.",/*comment*/ "Generic HTTP response")};
}

Result Result::notFound(std::optional<QString> customMessage)
{
    return Result{
        ErrorId::notFound,
        customMessage ? *customMessage : tr("Not found.",/*comment*/ "Generic HTTP response")};
}

Result Result::internalServerError(std::optional<QString> customMessage)
{
    return Result{ErrorId::internalServerError, customMessage
        ? *customMessage
        : tr("Internal error.")};
}

Result Result::unsupportedMediaType(std::optional<QString> customMessage)
{
    return Result{ErrorId::unsupportedMediaType, customMessage
        ? *customMessage
        : tr("Unsupported media type.")};
}

Result Result::serviceUnavailable(std::optional<QString> customMessage)
{
    return Result{ErrorId::serviceUnavailable, customMessage ? *customMessage : tr("Service unavailable.")};
}

Result Result::serviceUnauthorized(std::optional<QString> customMessage)
{
    return Result{ErrorId::serviceUnauthorized, customMessage ? *customMessage : tr("Service unauthorized.")};
}

Result Result::unauthorized(std::optional<QString> customMessage)
{
    return Result{ErrorId::unauthorized, customMessage ? *customMessage : tr("Unauthorized.")};
}

Result Result::sessionExpired(std::optional<QString> customMessage)
{
    return Result{ErrorId::sessionExpired, customMessage ? *customMessage : tr("Session expired.")};
}

Result Result::sessionRequired(std::optional<QString> customMessage)
{
    return Result{ErrorId::sessionRequired,
        customMessage ? *customMessage : tr("Session authorization required.")};
}

Result Result::sessionTruncated(std::optional<QString> customMessage)
{
    return Result{ErrorId::sessionTruncated,
        customMessage ? *customMessage : tr("Session is too old according to the Site config.")};
}

Result Result::gone(std::optional<QString> customMessage)
{
    return Result{ErrorId::gone,
        customMessage ? *customMessage : tr("Resource no longer present on server.")};
}

void serialize(QnJsonContext* /*context*/, const Result& value, QJsonValue* outTarget)
{
    *outTarget = QJsonObject{
        {"error", QJsonValue(QString::number(static_cast<int>(value.errorId)))},
        {"errorId", QJsonValue(QString::fromStdString(nx::reflect::toString(value.errorId)))},
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
        outTarget->errorId = static_cast<ErrorId>(error.toString().toInt(&isOk));
        if (!isOk)
        {
            context->setFailedKeyValue({"error", "Not an integer"});
            return false;
        }
    }
    else if (error.isDouble())
    {
        outTarget->errorId = static_cast<ErrorId>(error.toInt());
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

JsonResult::JsonResult(ErrorId error, QString errorString):
    Result(error, std::move(errorString))
{
}

void JsonResult::writeError(QByteArray* outBody, ErrorId error, const QString& errorMessage)
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

UbjsonResult::UbjsonResult(ErrorId error, QString errorString):
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
