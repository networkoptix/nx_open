#ifndef CONFIGURE_REST_HANDLER_H
#define CONFIGURE_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnConfigureRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
    virtual void afterExecute(const QString &path, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner) override;
private:
    int changeSystemName(const QString &systemName, qint64 sysIdTime, bool wholeSystem, qint64 remoteTranLogTime);
    int changeAdminPassword(
        const QString &password,
        const QByteArray &realm,
        const QByteArray &passwordHash,
        const QByteArray &passwordDigest,
        const QByteArray &cryptSha512Hash,
        const QString &oldPassword);
    int changePort(int port);
    void resetConnections();
};

#endif // CONFIGURE_REST_HANDLER_H
