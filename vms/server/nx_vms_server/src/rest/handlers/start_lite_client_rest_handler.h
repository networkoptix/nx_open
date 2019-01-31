#pragma once

#include "rest/server/json_rest_handler.h"
#include <nx/vms/server/server_module_aware.h>

class QnStartLiteClientRestHandler:
    public QnJsonRestHandler,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    QnStartLiteClientRestHandler(QnMediaServerModule* serverModule);

    /**
     * @return Whether Lite Client is present in the installation. The exact semantics of the check
     * is encapsulated inside the implementation of this method.
     */
    bool isLiteClientPresent() const;

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* connectionProcessor) override;
private:
    QString scriptFileName() const;
};
