#ifndef QN_BACKUP_CONTROL_REST_HANDLER_H
#define QN_BACKUP_CONTROL_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnBackupControlRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // QN_REBUILD_ARCHIVE_REST_HANDLER_H
