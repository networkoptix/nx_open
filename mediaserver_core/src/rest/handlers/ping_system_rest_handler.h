#ifndef PING_SYSTEM_REST_HANDLER_H
#define PING_SYSTEM_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>
#include <utils/network/module_information.h>

class QnPingSystemRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;

private:
    QnModuleInformation remoteModuleInformation(const QUrl &url, const QAuthenticator &auth, int &status);
    QnLicenseList remoteLicenses(const QUrl &url, const QAuthenticator &auth, int &status);
};

#endif // PING_SYSTEM_REST_HANDLER_H
