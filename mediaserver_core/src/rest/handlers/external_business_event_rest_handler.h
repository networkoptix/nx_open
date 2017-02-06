#ifndef QN_EXTERNAL_BUSINESS_EVENT_REST_HANDLER_H
#define QN_EXTERNAL_BUSINESS_EVENT_REST_HANDLER_H

#include <core/resource/resource_fwd.h>
#include "rest/server/json_rest_handler.h"

class QnExternalBusinessEventRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnExternalBusinessEventRestHandler();

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // QN_FILE_SYSTEM_HANDLER_H
