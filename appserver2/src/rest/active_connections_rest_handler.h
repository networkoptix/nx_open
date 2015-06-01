#ifndef QNACTIVECONNECTIONSRESTHANDLER_H
#define QNACTIVECONNECTIONSRESTHANDLER_H

#include "rest/server/json_rest_handler.h"

class QnActiveConnectionsRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // QNACTIVECONNECTIONSRESTHANDLER_H
