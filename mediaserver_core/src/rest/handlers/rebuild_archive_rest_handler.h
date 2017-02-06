#ifndef QN_REBUILD_ARCHIVE_REST_HANDLER_H
#define QN_REBUILD_ARCHIVE_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnRebuildArchiveRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // QN_REBUILD_ARCHIVE_REST_HANDLER_H
