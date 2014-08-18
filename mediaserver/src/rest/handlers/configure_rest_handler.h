#ifndef CONFIGURE_REST_HANDLER_H
#define CONFIGURE_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnConfigureRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result);

private:
    int changeSystemName(const QString &systemName, bool wholeSystem);
    int changeAdminPassword(const QString &password, const QByteArray &passwordHash, const QByteArray &passwordDigest);
    int changePort(int port);
    void resetConnections();
};

#endif // CONFIGURE_REST_HANDLER_H
