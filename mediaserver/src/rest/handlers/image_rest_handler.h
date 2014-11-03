#ifndef QN_IMAGE_REST_HANDLER_H
#define QN_IMAGE_REST_HANDLER_H

#include <QtCore/QByteArray>
#include "rest/server/request_handler.h"

class CLVideoDecoderOutput;
class QnVirtualCameraResource;
class QnServerArchiveDelegate;

class QnImageRestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    enum RoundMethod { IFrameBeforeTime, Precise, IFrameAfterTime };

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, 
                            QByteArray& contentType, const QnRestConnectionProcessor*) override;

private:
    int noVideoError(QByteArray& result, qint64 time);
    QSharedPointer<CLVideoDecoderOutput> readFrame(qint64 time, bool useHQ, RoundMethod roundMethod, const QSharedPointer<QnVirtualCameraResource>& res, QnServerArchiveDelegate& serverDelegate, int prefferedChannel);
    QSize updateDstSize(const QSharedPointer<QnVirtualCameraResource>& res, const QSize& dstSize, QSharedPointer<CLVideoDecoderOutput> outFrame);
private:
    bool m_detectAvailableOnly;
};

#endif // QN_IMAGE_REST_HANDLER_H
