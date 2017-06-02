#pragma once

#include <core/resource/resource_fwd.h>
#include <rest/server/json_rest_handler.h>

class QnExternalEventRestHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnExternalEventRestHandler();

    virtual int executeGet(const QString& path, const QnRequestParams& params,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner) override;
};
