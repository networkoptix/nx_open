#pragma once

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"
#include <nx/utils/timer_manager.h>
#include <nx/vms/server/server_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <nx/metrics/streams_metric_helper.h>

class QnFfmpegTranscoder;
class ProgressiveDownloadingServerPrivate;

class ProgressiveDownloadingServer: public QnTCPConnectionProcessor
{
public:
    static bool doesPathEndWithCameraId() { return true; } //< See the base class method.

    ProgressiveDownloadingServer(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner,
        QnMediaServerModule* serverModule);
    virtual ~ProgressiveDownloadingServer();

    QnFfmpegTranscoder* getTranscoder();

protected:
    virtual void run() override;

private:
    void setupPermissionsCheckTimer();
    void terminate(const QString& reason);
    static QByteArray getMimeType(const QByteArray& streamingFormat);
    void sendMediaEventErrorResponse(Qn::MediaStreamEvent mediaEvent);
    void sendJsonResponse(const QString& errorString);
    void processPositionRequest(
        const QnResourcePtr& resource, qint64 timeUSec, const QByteArray& callback);

private:
    Q_DECLARE_PRIVATE(ProgressiveDownloadingServer);
    nx::vms::metrics::StreamMetricHelper m_metricsHelper;
};
