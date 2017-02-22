#pragma once

#include "rest/server/json_rest_handler.h"

class QnUpdateUnauthenticatedRestHandler: public QnJsonRestHandler
{
public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* processor) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* processor) override;
};
