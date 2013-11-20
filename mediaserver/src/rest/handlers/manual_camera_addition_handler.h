#ifndef QN_MANUAL_CAMERA_ADDITION_HANDLER_H
#define QN_MANUAL_CAMERA_ADDITION_HANDLER_H

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>

#include <rest/server/json_rest_handler.h>

class QnManualCameraAdditionHandler: public QnJsonRestHandler {
public:
    QnManualCameraAdditionHandler();

protected:
    virtual int executeGet(const QString &path, const QnRequestParamList &params, JsonResult &result) override;
    virtual QString description() const;

private:
    int searchStartAction(const QnRequestParamList &params, JsonResult &result);
    int searchStatusAction(const QnRequestParamList &params, JsonResult &result);
    int searchStopAction(const QnRequestParamList &params, JsonResult &result);
    int addCamerasAction(const QnRequestParamList &params, JsonResult &result);
};

#endif // QN_MANUAL_CAMERA_ADDITION_HANDLER_H
