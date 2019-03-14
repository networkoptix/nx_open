#ifndef QN_JSON_AGGREGATOR_REST_HANDLER_H
#define QN_JSON_AGGREGATOR_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnJsonAggregatorRestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType, const QnRestConnectionProcessor* owner) override;
    virtual int executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, const QByteArray& srcBodyContentType, QByteArray &result, 
                            QByteArray &contentType, const QnRestConnectionProcessor*) override;
private:
    int executeGet(const QString &path, const QnRequestParamList &params, QnJsonRestResult &result, const QnRestConnectionProcessor*);
    bool executeCommad(const QString &command, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner, QVariantMap& fullData);
};

#endif // QN_JSON_AGGREGATOR_REST_HANDLER_H
