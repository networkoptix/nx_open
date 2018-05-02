#pragma once

#include <rest/server/json_rest_handler.h>

class QnGetHardwareIdsRestHandler: public QnJsonRestHandler
{
public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;
};
