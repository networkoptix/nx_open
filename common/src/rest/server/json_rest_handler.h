#ifndef QN_JSON_REST_HANDLER_H
#define QN_JSON_REST_HANDLER_H

#include "request_handler.h"

#include <utils/common/json.h>

class QnJsonRestResult {
public:
    QnJsonRestResult();

    const QString &errorText() const;
    void setErrorText(const QString &errorText);
    int errorId() const;
    void setErrorId(int errorId);
    void setError(int errorId, const QString &errorText);

    QVariant reply() const;

    template<class T>
    void setReply(const T &reply) {
        QJson::serialize(reply, &m_reply);
    }

    QByteArray serialize();

private:
    QString m_errorText;
    int m_errorId;
    QJsonValue m_reply;
};


class QnJsonRestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnJsonRestHandler();
    virtual ~QnJsonRestHandler();

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) = 0;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result);

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) override;
    virtual int executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) override;

private:
    QnRequestParams processParams(const QnRequestParamList &params);

private:
    QByteArray m_contentType;
};


#endif // QN_JSON_REST_HANDLER_H
