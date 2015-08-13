#ifndef QN_JSON_REST_RESULT_H
#define QN_JSON_REST_RESULT_H

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <utils/common/model_functions.h>

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
    };

    Error error;
    QString errorString;

    QnRestResult();
    void setError(Error errorValue, const QString &errorStringValue = QString());
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(QnRestResult::Error)

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
    T deserialized() {
        T result;
        QJson::deserialize(reply, &result);
        return result;
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
    T deserialized() {
        return QnUbjson::deserialized<T>(reply);
    }
};

#define QnUbjsonRestResult_Fields QnRestResult_Fields (reply)

#define QN_REST_RESULT_TYPES \
    (QnRestResult) \
    (QnJsonRestResult) \
    (QnUbjsonRestResult)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    QN_REST_RESULT_TYPES,
    (ubjson)(json)
    );

#endif // QN_JSON_REST_RESULT_H
