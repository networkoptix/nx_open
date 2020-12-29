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
    virtual int executePost(const QString& path, const QnRequestParams& params,
        const QByteArray& body, QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    // WARNING: Deprecated! Was left for a backward compatibility, should be removed in the future.
    virtual int executeGet(const QString& path, const QnRequestParams& params,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner) override;

private:
    int activateLicense(const QString& licenseKey, QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner);

    // Send request to the license server.
    CLHttpStatus makeRequest(QnCommonModule* commonModule, const QString& licenseKey, bool infoMode,
        QByteArray& response);
};

#endif // QN_ACTIVATE_LICENSE_REST_HANDLER_H
