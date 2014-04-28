#ifndef QN_JSON_REST_HANDLER_H
#define QN_JSON_REST_HANDLER_H

#include "request_handler.h"
#include "json_rest_result.h"

#include <utils/serialization/lexical.h>


class QnJsonRestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnJsonRestHandler();
    virtual ~QnJsonRestHandler();

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result);
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result);

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) override;
    virtual int executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) override;

    template<class T>
    bool requireParameter(const QnRequestParams &params, const QString &key, QnJsonRestResult &result, T *value, bool optional = false) const {
        auto pos = params.find(key);
        if(pos == params.end()) {
            if(optional) {
                return true;
            } else {
                result.setError(QnJsonRestResult::MissingParameter, lit("Parameter '%1' is missing.").arg(key));
                return false;
            }
        }

        if(!QnLexical::deserialize(*pos, value)) {
            result.setError(QnJsonRestResult::InvalidParameter, lit("Parameter '%1' has invalid value '%2'. Expected a value of type '%3'.").arg(key).arg(*pos).arg(QLatin1String(typeid(T).name())));
            return false;
        }

        return true;
    }

private:
    QnRequestParams processParams(const QnRequestParamList &params) const;

private:
    QByteArray m_contentType;
};


#endif // QN_JSON_REST_HANDLER_H
