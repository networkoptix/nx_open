// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>

namespace nx::network::rest {

/**%apidoc JSON object including the error status.
 * %param:enum errorId Mnemonic error id corresponding to the error code.
 *     %value ok
 *     %value missingParameter
 *     %value invalidParameter
 *     %value cantProcessRequest
 *     %value forbidden
 *     %value badRequest
 *     %value internalServerError
 *     %value conflict
 *     %value notImplemented
 *     %value notFound
 *     %value unsupportedMediaType
 *     %value serviceUnavailable
 *     %value unauthorized
 *     %value sessionExpired
 *
 * %// The typical apidoc comment for the result of a function looks like (without `//`):
 * %//return
 *     %//struct Result
 *     %//param[opt] reply
 *         %//struct _RestHandlerReply_
 */
struct NX_VMS_COMMON_API Result
{
    Q_DECLARE_TR_FUNCTIONS(Result)

    /** Template string with two parameters corresponding to name and value. */
    static QString invalidParameterTemplate();

public:
    // TODO: Move out and use NX_REFLECTION_ENUM.
    enum Error
    {
        /**%apidoc
         * %caption 0
         */
        NoError = 0,

        /**%apidoc
         * %caption 1
         */
        MissingParameter = 1,

        /**%apidoc
         * %caption 2
         */
        InvalidParameter = 2,

        /**%apidoc
         * %caption 3
         */
        CantProcessRequest = 3,

        /**%apidoc
         * %caption 4
         */
        Forbidden = 4,

        /**%apidoc
         * %caption 5
         */
        BadRequest = 5,

        /**%apidoc
         * %caption 6
         */
        InternalServerError = 6,

        /**%apidoc
         * %caption 7
         */
        Conflict = 7,

        /**%apidoc
         * %caption 8
         */
        NotImplemented = 8,

        /**%apidoc
         * %caption 9
         */
        NotFound = 9,

        /**%apidoc
         * %caption 10
         */
        UnsupportedMediaType = 10,

        /**%apidoc
         * %caption 11
         */
        ServiceUnavailable = 11,

        /**%apidoc
         * %caption 12
         */
        Unauthorized = 12,

        /**%apidoc
         * %caption 13
         */
        SessionExpired = 13,

        /**%apidoc
         * %caption 14
         */
        SessionRequired = 14,
    };

    static QString errorToString(Result::Error value);
    static nx::network::http::StatusCode::Value toHttpStatus(Error code);
    static Error errorFromHttpStatus(int status);

    /**%apidoc
     * Error code on failure (as an integer inside an enquoted string), or "0" on success.
     * %deprecated Use errorId instead.
     */
    Error error = Error::NoError;

    /**%apidoc Error message or an empty string. */
    QString errorString;

    Result(Error error = Error::NoError, QString errorString = "");

    static Result missingParameter(const QString& name);
    static Result cantProcessRequest(std::optional<QString> customMessage = std::nullopt);
    static Result forbidden(std::optional<QString> customMessage = std::nullopt);
    static Result conflict(std::optional<QString> customMessage = std::nullopt);
    static Result badRequest(std::optional<QString> customMessage = std::nullopt);
    static Result notImplemented(std::optional<QString> customMessage = std::nullopt);
    static Result notFound(std::optional<QString> customMessage = std::nullopt);
    static Result internalServerError(std::optional<QString> customMessage = std::nullopt);
    static Result unsupportedMediaType(std::optional<QString> customMessage = std::nullopt);
    static Result serviceUnavailable(std::optional<QString> customMessage = std::nullopt);
    static Result unauthorized(std::optional<QString> customMessage = std::nullopt);
    static Result sessionExpired(std::optional<QString> customMessage = std::nullopt);
    static Result sessionRequired(std::optional<QString> customMessage = std::nullopt);

    template<typename Value>
    static Result invalidParameter(const QString& name, const Value& value)
    {
        return Result{InvalidParameter,
            format(invalidParameterTemplate(), name, value)};
    }
};

NX_VMS_COMMON_API void serialize(const Result::Error& value, QString* target);
NX_VMS_COMMON_API bool deserialize(const QString& value, Result::Error* target);

NX_VMS_COMMON_API void serialize(
    QnJsonContext* context, const Result& value, QJsonValue* outTarget);
NX_VMS_COMMON_API bool deserialize(
    QnJsonContext* context, QnConversionWrapper<QJsonValue> value, Result* outTarget);

#define Result_Fields (error)(errorString)
QN_FUSION_DECLARE_FUNCTIONS(Result, (ubjson)(xml)(csv_record), NX_VMS_COMMON_API)

struct NX_VMS_COMMON_API JsonResult: public Result
{
public:
    JsonResult() = default;
    JsonResult(const Result& result);
    JsonResult(Error error, QString errorString);

    QJsonValue reply;

    template<typename T>
    void setReply(const T& data);

    template<typename T>
    T deserialized(bool* isOk = nullptr) const;

    /** Convenience function which creates serialized JSON result. */
    static void writeError(QByteArray* outBody, Error error, const QString& errorMessage);
};

NX_VMS_COMMON_API void serialize(
    QnJsonContext* context, const JsonResult& value, QJsonValue* outTarget);
NX_VMS_COMMON_API bool deserialize(
    QnJsonContext* context, QnConversionWrapper<QJsonValue> value, JsonResult* outTarget);

#define JsonResult_Fields Result_Fields (reply)
QN_FUSION_DECLARE_FUNCTIONS(JsonResult, (ubjson), NX_VMS_COMMON_API)

class NX_VMS_COMMON_API UbjsonResult: public Result
{
public:
    UbjsonResult() = default;
    UbjsonResult(const Result& result);
    UbjsonResult(Error error, QString errorString);

    QByteArray reply;

    template<typename T>
    void setReply(const T& replyStruct);

    template<typename T>
    T deserialized(bool* isOk = nullptr) const;
};

NX_VMS_COMMON_API void serialize(
    QnJsonContext* context, const UbjsonResult& value, QJsonValue* outTarget);
NX_VMS_COMMON_API bool deserialize(
    QnJsonContext* context, QnConversionWrapper<QJsonValue> value, UbjsonResult* outTarget);

#define UbjsonResult_Fields Result_Fields (reply)
QN_FUSION_DECLARE_FUNCTIONS(UbjsonResult, (ubjson), NX_VMS_COMMON_API)

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
