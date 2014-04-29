#ifndef QN_CAMERA_BOOKMARKS_REST_HANDLER_H
#define QN_CAMERA_BOOKMARKS_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>

class QnCameraBookmarksRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) override;

    virtual QString description() const override;

private:
    int addCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result);
};

#endif // QN_CAMERA_BOOKMARKS_REST_HANDLER_H
