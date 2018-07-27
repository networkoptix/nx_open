#pragma once

#include <rest/server/fusion_rest_handler.h>

class QnInstallUpdateRestHandler : public QnFusionRestHandler
{
public:
    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;
};
