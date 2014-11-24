#ifndef MERGE_SYSTEMS_REST_HANDLER_H
#define MERGE_SYSTEMS_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnMergeSystemsRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner) override;
private:
    bool changeSystemName(const QString &systemName);
    bool changeAdminPassword(const QString &password);
};

#endif // MERGE_SYSTEMS_REST_HANDLER_H
