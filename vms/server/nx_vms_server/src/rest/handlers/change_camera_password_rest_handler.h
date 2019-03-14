#pragma once

#include <rest/server/json_rest_handler.h>

class QnChangeCameraPasswordRestHandler: public QnJsonRestHandler
{
public:
    virtual int executePost(
        const QString &path,
        const QnRequestParams &params,
        const QByteArray &body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;
};
