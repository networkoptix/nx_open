#ifndef CHANGE_SYSTEM_NAME_REST_HANDLER_H
#define CHANGE_SYSTEM_NAME_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

//! This is a temporary handler for AFN. It will be removed as soon as we merge network_issues into the default branch.

class QnChangeSystemNameRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*);
};

#endif // CHANGE_SYSTEM_NAME_REST_HANDLER_H
