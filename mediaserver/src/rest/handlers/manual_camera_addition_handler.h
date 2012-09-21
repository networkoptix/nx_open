#ifndef QN_MANUAL_CAMERA_ADDITION_HANDLER_H
#define QN_MANUAL_CAMERA_ADDITION_HANDLER_H

#include "rest/server/request_handler.h"

class QnManualCameraAdditionHandler: public QnRestRequestHandler
{
public:
    QnManualCameraAdditionHandler();
protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& resultByteArray, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
private:
    int searchAction(const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType);
    int addAction(const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType);
};

#endif // QN_MANUAL_CAMERA_ADDITION_HANDLER_H
