#pragma once


#include "rest/server/json_rest_handler.h"
#include "rest/server/fusion_rest_handler.h"
#include <nx/mediaserver/server_module_aware.h>

class QnAuditLogRestHandler: public QnFusionRestHandler, public nx::mediaserver::ServerModuleAware
{
public:
    QnAuditLogRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path, 
        const QnRequestParamList& params, 
        QByteArray& result, 
        QByteArray& contentType, 
        const QnRestConnectionProcessor*) override;
};
