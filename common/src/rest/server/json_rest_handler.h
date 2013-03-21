#ifndef QN_JSON_REST_HANDLER_H
#define QN_JSON_REST_HANDLER_H

#include "request_handler.h"

#include <utils/common/json.h>

class QnJsonRestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    class JsonResult {
    public:
        JsonResult(): m_errorId(-1) {}

        const QString &errorText() const {
            return m_errorText;
        }

        void setErrorText(const QString &errorText) {
            m_errorText = errorText;
        }

        int errorId() const {
            return m_errorId;
        }

        void setErrorId(int errorId) {
            m_errorId = errorId;
        }

        void setError(int errorId, const QString &errorText) {
            m_errorId = errorId;
            m_errorText = errorText;
        }

        template<class T>
        void setReply(const T &reply) {
            QJson::serialize(reply, &m_reply);
        }

        QVariant reply() const {
            return m_reply;
        }

        QByteArray serialize() {
            QVariantMap map;
            if(!m_errorText.isEmpty())
                map[QLatin1String("errorText")] = m_errorText;
            if(m_errorId != -1)
                map[QLatin1String("errorId")] = m_errorId;
            map[QLatin1String("reply")] = m_reply;

            QByteArray result;
            QJson::serialize(map, &result);
            return result;
        }

    private:
        QString m_errorText;
        int m_errorId;
        QVariant m_reply;
    };

    QnJsonRestHandler(): m_contentType("application/json") {}

    virtual int executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) override {
        JsonResult jsonResult;
        int returnCode = executeGet(path, params, jsonResult);

        result = jsonResult.serialize();
        contentType = m_contentType;

        return returnCode;
    }

    virtual int executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) override {
        JsonResult jsonResult;
        int returnCode = executePost(path, params, body, jsonResult);
        
        result = jsonResult.serialize();
        contentType = m_contentType;

        return returnCode;
    }

    virtual int executeGet(const QString &path, const QnRequestParamList &params, JsonResult &result) = 0;
    
    virtual int executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, JsonResult &result) {
        Q_UNUSED(body);
        return executeGet(path, params, result);
    }

private:
    QByteArray m_contentType;
};


#endif // QN_JSON_REST_HANDLER_H
