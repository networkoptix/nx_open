#ifndef QN_JSON_REST_RESULT_H
#define QN_JSON_REST_RESULT_H

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <utils/common/model_functions.h>


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

class QnRestResult {
public:
    enum Error {
        NoError = 0,
        MissingParameter = 1,
        InvalidParameter = 2,
        CantProcessRequest = 3,
    };

    QnRestResult();

    const QString &errorString() const;
    void setErrorString(const QString &errorString);
    Error error() const;
    void setError(Error error);
    void setError(Error error, const QString &errorString);

protected:
    QN_FUSION_ENABLE_PRIVATE();
    QString m_errorString;
    Error m_error;
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(QnRestResult::Error)


class QnJsonRestResult: public QnRestResult {
public:
    QnJsonRestResult(): QnRestResult() {}
    QnJsonRestResult(const QnRestResult& base): QnRestResult(base) {}


    const QJsonValue &reply() const { return m_reply; }

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
    QJsonValue m_reply;
};

class QnUbjsonRestResult: public QnRestResult {
public:
    QnUbjsonRestResult(): QnRestResult() {}
    QnUbjsonRestResult(const QnRestResult& base): QnRestResult(base) {}


    const QByteArray &reply() const { return m_reply; }

    template<class T>
    void setReply(const T &reply) {
        m_reply = QnUbjson::serialized(reply);
    }

    void setReply(const QByteArray &reply) {
        m_reply = reply;
    }

    QN_FUSION_DECLARE_FUNCTIONS(QnUbjsonRestResult, (ubjson), friend)

private:
    QN_FUSION_ENABLE_PRIVATE();
    QByteArray m_reply;
};

#endif // QN_JSON_REST_RESULT_H
