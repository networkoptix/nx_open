#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/mediaserver/server_module_aware.h>

class QnProxyDesktopDataProvider;

class QnAudioTransmissionRestHandler: 
    public QnJsonRestHandler,
    public nx::mediaserver::ServerModuleAware
{
public:
    QnAudioTransmissionRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    static bool validateParams(const QnRequestParams& params, QString& error);
};

