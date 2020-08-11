#pragma once

#include <core/resource/resource_fwd.h>

#include <rest/server/fusion_rest_handler.h>

#include <api/helpers/request_helpers_fwd.h>
#include <nx/vms/server/server_module_aware.h>

class QnCommonModule;

class QnMultiserverThumbnailRestHandler:
    public QnFusionRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnMultiserverThumbnailRestHandler(
        QnMediaServerModule* serverModule,
        const QString& path = QString());
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor *processor) override;

    // todo: remove ownerPort from params
    int getScreenshot(
        QnCommonModule* commonModule,
        const QnThumbnailRequestData &request,
        QByteArray& result,
        QByteArray& contentType,
        int ownerPort,
        qint64* frameTimestamsUsec = nullptr);

private:
    int getScreenshot(
        QnCommonModule* commonModule,
        const QnThumbnailRequestData& request,
        QByteArray& result,
        QByteArray& contentType,
        int ownerPort,
        nx::network::http::HttpHeaders* outExtraHeaders);

    /** Calculate server where the request should be executed. */
    QnMediaServerResourcePtr targetServer(QnCommonModule* commonModule,
        const QnThumbnailRequestData &request) const;

    int getThumbnailLocal(const QnThumbnailRequestData &request, QByteArray& result,
        QByteArray& contentType, nx::network::http::HttpHeaders* outExtraHeaders) const;
    int getThumbnailRemote(const QnMediaServerResourcePtr &server,
        const QnThumbnailRequestData &request, QByteArray& result, QByteArray& contentType,
        int ownerPort, nx::network::http::HttpHeaders* outExtraHeaders) const;

    int getThumbnailFromArchive(
        const QnThumbnailRequestData& request,
        QByteArray& result,
        QByteArray& contentType,
        nx::network::http::HttpHeaders* outExtraHeaders) const;

    int getThumbnailForAnalyticsTrack(
        const QnThumbnailRequestData& request,
        QByteArray& result,
        QByteArray& contentType,
        nx::network::http::HttpHeaders* outExtraHeaders) const;

};
