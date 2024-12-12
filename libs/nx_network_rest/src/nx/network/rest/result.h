// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/enum_instrument.h>

namespace nx::network::rest {

NX_REFLECTION_ENUM_CLASS(ErrorId,
        ok = 0,
        missingParameter = 1,
        invalidParameter = 2,
        cantProcessRequest = 3,
        forbidden = 4,
        badRequest = 5,
        internalServerError = 6,
        conflict = 7,
        notImplemented = 8,
        notFound = 9,
        unsupportedMediaType = 10,
        serviceUnavailable = 11,
        unauthorized = 12,
        sessionExpired = 13,
        sessionRequired = 14,
        sessionTruncated = 15,
        gone = 16,
        notAllowed = 17,
        serviceUnauthorized = 18
)

/**%apidoc JSON object including the error status.
 * %param:string error Numeric string representation of `errorId`.
 *     %value "0" ok
 *     %value "1" missingParameter
 *     %value "2" invalidParameter
 *     %value "3" cantProcessRequest
 *     %value "4" forbidden
 *     %value "5" badRequest
 *     %value "6" internalServerError
 *     %value "7" conflict
 *     %value "8" notImplemented
 *     %value "9" notFound
 *     %value "10" unsupportedMediaType
 *     %value "11" serviceUnavailable
 *     %value "12" unauthorized
 *     %value "13" sessionExpired
 *     %value "14" sessionRequired
 *     %value "15" sessionTruncated
 *     %value "16" gone
 *     %value "17" notAllowed
 *     %value "18" serviceUnauthorized
 *     %deprecated Use `errorId` instead.
 *
 * %// The typical apidoc comment for the result of a function looks like (without `//`):
 * %//return
 *     %//struct Result
 *     %//param[opt] reply
 *         %//struct _RestHandlerReply_
 */
struct NX_NETWORK_REST_API Result
{
    Q_DECLARE_TR_FUNCTIONS(Result)

    /** Template string with two parameters corresponding to name and value. */
    static QString invalidParameterTemplate();

public:

    static nx::network::http::StatusCode::Value toHttpStatus(ErrorId code);
    static ErrorId errorFromHttpStatus(int status);

    ErrorId errorId = ErrorId::ok;

    /**%apidoc Error message or an empty string. */
    QString errorString;

    Result(ErrorId error = ErrorId::ok, QString errorString = "");

    static Result missingParameter(const QString& name);
    static Result cantProcessRequest(std::optional<QString> customMessage = std::nullopt);
    static Result forbidden(std::optional<QString> customMessage = std::nullopt);
    static Result conflict(std::optional<QString> customMessage = std::nullopt);
    static Result badRequest(std::optional<QString> customMessage = std::nullopt);
    static Result notAllowed(std::optional<QString> customMessage = std::nullopt);
    static Result notImplemented(std::optional<QString> customMessage = std::nullopt);
    static Result notFound(std::optional<QString> customMessage = std::nullopt);
    static Result internalServerError(std::optional<QString> customMessage = std::nullopt);
    static Result unsupportedMediaType(std::optional<QString> customMessage = std::nullopt);
    static Result serviceUnavailable(std::optional<QString> customMessage = std::nullopt);
    static Result serviceUnauthorized(std::optional<QString> customMessage = std::nullopt);
    static Result unauthorized(std::optional<QString> customMessage = std::nullopt);
    static Result sessionExpired(std::optional<QString> customMessage = std::nullopt);
    static Result sessionRequired(std::optional<QString> customMessage = std::nullopt);
    static Result sessionTruncated(std::optional<QString> customMessage = std::nullopt);
    static Result gone(std::optional<QString> customMessage = std::nullopt);

    template<typename Value>
    static Result invalidParameter(const QString& name, const Value& value)
    {
        return Result{ErrorId::invalidParameter,
            format(invalidParameterTemplate(), name, value)};
    }
};

NX_NETWORK_REST_API void serialize(
    QnJsonContext* context, const Result& value, QJsonValue* outTarget);
NX_NETWORK_REST_API bool deserialize(
    QnJsonContext* context, QnConversionWrapper<QJsonValue> value, Result* outTarget);

#define Result_Fields (errorId)(errorString)
QN_FUSION_DECLARE_FUNCTIONS(Result, (ubjson)(xml)(csv_record), NX_NETWORK_REST_API)

struct NX_NETWORK_REST_API JsonResult: public Result
{
public:
    JsonResult() = default;
    JsonResult(const Result& result);
    JsonResult(ErrorId error, QString errorString);

    QJsonValue reply;

    template<typename T>
    void setReply(const T& data);

    template<typename T>
    T deserialized(bool* isOk = nullptr) const;

    /** Convenience function which creates serialized JSON result. */
    static void writeError(QByteArray* outBody, ErrorId error, const QString& errorMessage);
};

NX_NETWORK_REST_API void serialize(
    QnJsonContext* context, const JsonResult& value, QJsonValue* outTarget);
NX_NETWORK_REST_API bool deserialize(
    QnJsonContext* context, QnConversionWrapper<QJsonValue> value, JsonResult* outTarget);

#define JsonResult_Fields Result_Fields (reply)
QN_FUSION_DECLARE_FUNCTIONS(JsonResult, (ubjson), NX_NETWORK_REST_API)

class NX_NETWORK_REST_API UbjsonResult: public Result
{
public:
    UbjsonResult() = default;
    UbjsonResult(const Result& result);
    UbjsonResult(ErrorId error, QString errorString);

    QByteArray reply;

    template<typename T>
    void setReply(const T& replyStruct);

    template<typename T>
    T deserialized(bool* isOk = nullptr) const;
};

NX_NETWORK_REST_API void serialize(
    QnJsonContext* context, const UbjsonResult& value, QJsonValue* outTarget);
NX_NETWORK_REST_API bool deserialize(
    QnJsonContext* context, QnConversionWrapper<QJsonValue> value, UbjsonResult* outTarget);

#define UbjsonResult_Fields Result_Fields (reply)
QN_FUSION_DECLARE_FUNCTIONS(UbjsonResult, (ubjson), NX_NETWORK_REST_API)

//--------------------------------------------------------------------------------------------------

template<typename T>
void JsonResult::setReply(const T &data)
{
    QJson::serialize(data, &reply);
}

template<typename T>
T JsonResult::deserialized(bool* isOk) const
{
    bool uselessBool = false;
    if (!isOk)
        isOk = &uselessBool;

    T value;
    *isOk = QJson::deserialize(reply, &value);
    return value;
}

template<typename T>
void UbjsonResult::setReply(const T& data)
{
    QnUbjson::serialize(data, &reply);
}

template<typename T>
T UbjsonResult::deserialized(bool* isOk) const
{
    return QnUbjson::deserialized<T>(reply, T(), isOk);
}

} // namespace nx::network::rest
