#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>
#include "private/multiserver_bookmarks_rest_handler_p.h"

class QnMultiserverBookmarksRestHandler:
    public QnFusionRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnMultiserverBookmarksRestHandler(QnMediaServerModule* serverModule, const QString& path);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* processor) override;
};
