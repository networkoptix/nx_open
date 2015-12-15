#pragma once

#include <core/resource/resource_fwd.h>

#include <rest/server/fusion_rest_handler.h>

#include <api/helpers/request_helpers_fwd.h>

class QnMultiserverThumbnailRestHandler: public QnFusionRestHandler
{
public:
    QnMultiserverThumbnailRestHandler(const QString& path);
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor *processor) override;

private:
    /** Calculate server where the request should be executed. */
    QnMediaServerResourcePtr targetServer(const QnThumbnailRequestData &request) const;

    int getThumbnailLocal(const QnThumbnailRequestData &request, QByteArray& result, QByteArray& contentType) const;
    int getThumbnailRemote(const QnMediaServerResourcePtr &server, const QnThumbnailRequestData &request, QByteArray& result, QByteArray& contentType, int ownerPort) const;
};
