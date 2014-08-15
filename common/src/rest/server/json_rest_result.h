#ifndef QN_JSON_REST_RESULT_H
#define QN_JSON_REST_RESULT_H

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <utils/serialization/json.h>
#include <utils/common/model_functions_fwd.h>

// TODO: #MSAPI rename to QnRestResult.
// 
// Add format field (Qn::SerializationFormat) that will be set in QnJsonRestHandler (to be renamed).
// In setReply do QByteArray serialization right away, don't store in an 
// intermediate QVariant/QJsonValue.
// 
// And it also might make sense to get rid of "error" in reply. Looks like it 
// was a bad idea in the first place. This way we'll send a reply if there was
// no error, and some HTTP error code otherwise. And maybe a text/plain 
// description of the error.
// 

class QnJsonRestResult {
public:
    enum Error {
        NoError = 0,
        MissingParameter = 1,
        InvalidParameter = 2,
        CantProcessRequest = 3,
    };

    QnJsonRestResult();

    const QString &errorString() const;
    void setErrorString(const QString &errorString);
    Error error() const;
    void setError(Error error);
    void setError(Error error, const QString &errorString);

    const QJsonValue &reply() const;

    template<class T>
    void setReply(const T &reply) {
        QJson::serialize(reply, &m_reply);
    }

    void setReply(const QJsonValue &reply) {
        m_reply = reply;
    }

    QN_FUSION_DECLARE_FUNCTIONS(QnJsonRestResult, (json), friend)

private:
    QN_FUSION_ENABLE_PRIVATE();

    QString m_errorString;
    Error m_error;
    QJsonValue m_reply;
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(QnJsonRestResult::Error)

#endif // QN_JSON_REST_RESULT_H
