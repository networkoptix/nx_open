#ifndef JOIN_SYSTEM_REST_HANDLER_H
#define JOIN_SYSTEM_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnJoinSystemRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result);
private:
    bool changeSystemName(const QString &systemName);
    bool changeAdminPassword(const QString &password);
};

#endif // JOIN_SYSTEM_REST_HANDLER_H
