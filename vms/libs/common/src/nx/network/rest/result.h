#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>

namespace nx::network::rest {

struct Result
{
public:
    enum Error {
        NoError = 0,
        MissingParameter = 1,
        InvalidParameter = 2,
        CantProcessRequest = 3,
        Forbidden = 4,
        BadRequest = 5,
        InternalServerError = 6,
    };

    static nx::network::http::StatusCode::Value toHttpStatus(Error code);

    /**
     * Presents error as corresponding text with some arguments.
     * E.g., ErrorDescriptor(MissingParameter, "id").text() will return text like
     * "Missing required parameter 'id'".
     * Introduced for error text unification.
     */
    class ErrorDescriptor
    {
    public:
        template<typename... Args>
        ErrorDescriptor(Error code, Args... args);

        Error code() const { return m_code; }
        QString text() const;

    private:
        Error m_code;
        QStringList m_arguments;
    };

    Error error = Error::NoError;
    QString errorString;

    void setError(Error error_, QString errorString_ = {});
    void setError(const ErrorDescriptor& description);
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Result::Error)

#define Result_Fields (error)(errorString)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Result), (ubjson)(xml)(json)(csv_record))

struct JsonResult: public Result
{
public:
    JsonResult(const Result& result = {});

    QJsonValue reply;

    template<typename T>
    void setReply(const T& data);

    template<typename T>
    T deserialized(bool* isOk = nullptr) const;

    /** Convenience function which creates serialized JSON result. */
    static void writeError(QByteArray* outBody, Error error, const QString& errorMessage);
};

class UbjsonResult: public Result
{
public:
    UbjsonResult(const Result& result = {});

    QByteArray reply;

    template<typename T>
    void setReply(const T& replyStruct);

    template<typename T>
    T deserialized(bool* isOk = nullptr) const;
};

#define JsonResult_Fields Result_Fields (reply)
#define UbjsonResult_Fields Result_Fields (reply)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((JsonResult)(UbjsonResult), (ubjson)(json))

//--------------------------------------------------------------------------------------------------

inline void qStringListAdd(QStringList* /*list*/)
{
}

template<typename... Args>
void qStringListAdd(QStringList* list, QString arg, Args... args)
{
    list->append(std::move(arg));
    qStringListAdd(list, std::forward<Args>(args)...);
}

template<typename... Args>
QStringList qStringListInit(Args... args)
{
    QStringList argList;
    qStringListAdd(&argList, std::forward<Args>(args)...);
    return argList;
}

template<typename... Args>
Result::ErrorDescriptor::ErrorDescriptor(Error code, Args... args):
    m_code(code),
    m_arguments(qStringListInit(std::forward<Args>(args)...))
{
}

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

QN_FUSION_DECLARE_FUNCTIONS(nx::network::rest::Result::Error, (lexical)(numeric))
