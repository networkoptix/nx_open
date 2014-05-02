#ifndef QN_JSON_REST_RESULT_H
#define QN_JSON_REST_RESULT_H

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <utils/serialization/json.h>

// TODO: #MSAPI rename to QnRestResult.
// 
// Add format field (Qn::SerializationFormat) that will be set in QnJsonRestHandler (to be renamed).
// In setReply do QByteArray serialization right away, don't store in an 
// intermediate QVariant/QJsonValue.

class QnJsonRestResult {
public:
    enum Error {
        NoError = 0,
        MissingParameter = 1,
        InvalidParameter = 2,
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

    QN_FUSION_DECLARE_FUNCTIONS(QnJsonRestResult, (json), friend)

private:
    QN_FUSION_ENABLE_PRIVATE();

    QString m_errorString;
    Error m_error;
    QJsonValue m_reply;
};


#endif // QN_JSON_REST_RESULT_H
