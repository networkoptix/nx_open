#pragma once

#include <QtCore/QRect>

#include <rest/server/request_handler.h>
#include <nx/vms/server/server_module_aware.h>

class QnRecordedChunksRestHandler:
    public QnRestRequestHandler,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    QnRecordedChunksRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* /*owner*/) override;

    virtual int executePost(
        const QString& path, const QnRequestParamList& params, const QByteArray& body,
        const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType,
        const QnRestConnectionProcessor* /*owner*/) override;

private:
    // TODO: #Elric #enum
    enum ChunkFormat
    {
        ChunkFormat_Unknown,
        ChunkFormat_Binary,
        ChunkFormat_XML,
        ChunkFormat_Json,
        ChunkFormat_Text,
    };
};
