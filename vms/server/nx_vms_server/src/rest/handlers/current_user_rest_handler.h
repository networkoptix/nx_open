#ifndef __QN_CURRENT_USER_REST_HANDLER_H_
#define __QN_CURRENT_USER_REST_HANDLER_H_

#include <rest/server/json_rest_handler.h>

class QnCurrentUserRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    using nx::network::rest::Handler::executeGet;
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // __QN_CURRENT_USER_REST_HANDLER_H_
