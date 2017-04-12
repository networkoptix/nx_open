#pragma once

#include <rest/server/json_rest_handler.h>

class QnDebugEventsRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;

private:
    int testNetworkIssue(
        const QnRequestParams & params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner);
    int testCameraDisconnected(
        const QnRequestParams & params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner);

};
