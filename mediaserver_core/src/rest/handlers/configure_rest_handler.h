#ifndef CONFIGURE_REST_HANDLER_H
#define CONFIGURE_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

struct ConfigureSystemData;
class QnConfigureRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    int execute(const ConfigureSystemData& data, QnJsonRestResult &result, const QnRestConnectionProcessor* owner);
    int changePort(const QnUuid & userId, int port);
};

#endif // CONFIGURE_REST_HANDLER_H
