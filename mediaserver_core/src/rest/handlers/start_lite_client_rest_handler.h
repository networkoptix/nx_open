#pragma once

#include "rest/server/json_rest_handler.h"

class QnStartLiteClientRestHandler
:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* connectionProcessor) override;
};
