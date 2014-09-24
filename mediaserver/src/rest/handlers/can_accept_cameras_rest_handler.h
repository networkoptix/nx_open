#ifndef QN_CAN_ACCEPT_CAMERA_REST_HANDLER_H
#define QN_CAN_ACCEPT_CAMERA_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"


class QnCanAcceptCameraRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) override;
};

#endif // QN_CAN_ACCEPT_CAMERA_REST_HANDLER_H
