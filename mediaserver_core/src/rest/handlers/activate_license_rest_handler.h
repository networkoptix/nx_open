#ifndef QN_ACTIVATE_LICENSE_REST_HANDLER_H
#define QN_ACTIVATE_LICENSE_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/deprecated/simple_http_client.h>

class QnCommonModule;

class QnActivateLicenseRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*owner) override;
private:
    CLHttpStatus makeRequest(QnCommonModule* commonModule, const QString& licenseKey, bool infoMode, QByteArray& response);
};

#endif // QN_ACTIVATE_LICENSE_REST_HANDLER_H
