#ifndef IFLIST_REST_HANDLER_H
#define IFLIST_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"

class QnIfListRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // IFLIST_REST_HANDLER_H
