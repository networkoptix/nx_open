#pragma once

#include <core/resource/resource_fwd.h>

#include <rest/server/fusion_rest_handler.h>

#include <api/helpers/request_helpers_fwd.h>

class QnCommonModule;

class QnMultiserverThumbnailRestHandler: public QnFusionRestHandler
{
public:
    QnMultiserverThumbnailRestHandler(const QString& path = QString());
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor *processor) override;

    // todo: remove ownerPort from params
    int getScreenshot(
        QnCommonModule* commonModule,
        const QnThumbnailRequestData &request,
        QByteArray& result,
        QByteArray& contentType,
        int ownerPort);

private:
    /** Calculate server where the request should be executed. */
    QnMediaServerResourcePtr targetServer(QnCommonModule* commonModule, const QnThumbnailRequestData &request) const;

    int getThumbnailLocal(const QnThumbnailRequestData &request, QByteArray& result, QByteArray& contentType) const;
    int getThumbnailRemote(const QnMediaServerResourcePtr &server, const QnThumbnailRequestData &request, QByteArray& result, QByteArray& contentType, int ownerPort) const;
};
