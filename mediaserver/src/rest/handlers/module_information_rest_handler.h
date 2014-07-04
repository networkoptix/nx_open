#ifndef MODULE_INFORMATION_REST_HANDLER_H
#define MODULE_INFORMATION_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"

class QnModuleInformationRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
};

#endif // MODULE_INFORMATION_REST_HANDLER_H
