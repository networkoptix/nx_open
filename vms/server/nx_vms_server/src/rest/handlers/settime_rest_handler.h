#ifndef SETTIME_REST_HANDLER_H
#define SETTIME_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"

struct SetTimeData;

class QnSetTimeRestHandler : public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray &body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner) override;

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;
private:
    int execute(
        const SetTimeData& data,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult& result);
};

#endif // SETTIME_REST_HANDLER_H
