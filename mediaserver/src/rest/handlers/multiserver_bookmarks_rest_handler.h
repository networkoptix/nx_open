#pragma once

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <rest/server/fusion_rest_handler.h>

class QnMultiserverBookmarksRestHandler: public QnFusionRestHandler
{
public:
    QnMultiserverBookmarksRestHandler(const QString& path);
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;

    static QnCameraBookmarkList loadDataSync(const QnBookmarkRequestData& request);
private:
    struct InternalContext;
    static void waitForDone(InternalContext* ctx);
    static void loadRemoteDataAsync(MultiServerCameraBookmarkList& outputData, const QnMediaServerResourcePtr &server, InternalContext* ctx);
    static void loadLocalData(MultiServerCameraBookmarkList& outputData, InternalContext* ctx);
};
