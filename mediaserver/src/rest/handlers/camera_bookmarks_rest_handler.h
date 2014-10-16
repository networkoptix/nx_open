#ifndef QN_CAMERA_BOOKMARKS_REST_HANDLER_H
#define QN_CAMERA_BOOKMARKS_REST_HANDLER_H

#include <core/resource/camera_bookmark.h>

#include <rest/server/json_rest_handler.h>

class QnCameraBookmarksRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;

private:
    int addCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result);
    int updateCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result);
    int deleteCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result);
    int getCameraBookmarksAction(const QnRequestParams & params, QnJsonRestResult &result);
};

#endif // QN_CAMERA_BOOKMARKS_REST_HANDLER_H
