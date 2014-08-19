#ifndef BACKUP_DB_REST_HANDLER_H
#define BACKUP_DB_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnBackupDbRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result);
};

#endif // BACKUP_DB_REST_HANDLER_H
