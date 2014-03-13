#ifndef QN_REBUILD_ARCHIVE_REST_HANDLER_H
#define QN_REBUILD_ARCHIVE_REST_HANDLER_H

#include "rest/server/request_handler.h"

class QnRebuildArchiveRestHandler: public QnRestRequestHandler
{
public:
    QnRebuildArchiveRestHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const;
};

#endif // QN_REBUILD_ARCHIVE_REST_HANDLER_H
