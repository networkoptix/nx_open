#ifndef QN_JSON_REST_RESULT_H
#define QN_JSON_REST_RESULT_H

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/fusion/model_functions.h>

// TODO: #MSAPI rename module to rest_result
// Add format field (Qn::SerializationFormat) that will be set in QnJsonRestHandler (to be renamed).
//
// And it also might make sense to get rid of "error" in reply. Looks like it
// was a bad idea in the first place. This way we'll send a reply if there was
// no error, and some HTTP error code otherwise. And maybe a text/plain
// description of the error.
//

struct QnRestResult {
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

    /** Presents error as corresponding text with some arguments.
        E.g., ErrorDescriptor(MissingParameter, "id").text()
            will return text like "Missing required parameter 'id'".
        \note Introduced for error text unification
    */
    class ErrorDescriptor
    {
    public:
        ErrorDescriptor(Error errorCode, QString argument);
        ErrorDescriptor(Error errorCode, QStringList arguments);

        Error errorCode() const;
        QString text() const;

    private:
        Error m_errorCode;
        QStringList m_arguments;
    };

    Error error;
    QString errorString;

    QnRestResult();
    void setError(Error errorValue, const QString &errorStringValue = QString());
    void setError(const ErrorDescriptor& errorDescriptor);
};
#define QnRestResult_Fields (error)(errorString)

struct QnJsonRestResult: public QnRestResult {
public:
    QnJsonRestResult();
    QnJsonRestResult(const QnRestResult& base);

    QJsonValue reply;

    template<class T>
    void setReply(const T &replyStruct) {
        QJson::serialize(replyStruct, &reply);
    }

    template<class T>
    T deserialized() const {
        T result;
        QJson::deserialize(reply, &result);
        return result;
    }

    /**
     * Convenience function which creates serialized JSON result.
     */
    static void writeError(QByteArray* outBody, Error error, const QString& errorMessage) {
        QnJsonRestResult jsonRestResult;
        jsonRestResult.setError(error, errorMessage);
        *outBody = QJson::serialized(jsonRestResult);
    }
};

#define QnJsonRestResult_Fields QnRestResult_Fields (reply)

class QnUbjsonRestResult: public QnRestResult {
public:
    QnUbjsonRestResult();
    QnUbjsonRestResult(const QnRestResult& base);

    QByteArray reply;

    template<class T>
    void setReply(const T &replyStruct) {
        QnUbjson::serialize(replyStruct, &reply);
    }

    template<class T>
    T deserialized(bool* success = nullptr)
    {
        return QnUbjson::deserialized<T>(reply, T(), success);
    }
};

#define QnUbjsonRestResult_Fields QnRestResult_Fields (reply)

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(QnRestResult::Error)
QN_FUSION_DECLARE_FUNCTIONS(QnRestResult::Error, (lexical)(numeric))
QN_FUSION_DECLARE_FUNCTIONS(QnRestResult, (ubjson)(xml)(json)(csv_record))

#define QN_REST_RESULT_TYPES \
    (QnJsonRestResult) \
    (QnUbjsonRestResult)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    QN_REST_RESULT_TYPES,
    (ubjson)(json)
    );

#endif // QN_JSON_REST_RESULT_H
