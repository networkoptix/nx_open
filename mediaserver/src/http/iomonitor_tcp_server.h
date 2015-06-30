#ifndef QN_IOMONITOR_REST_HANDLER_H
#define QN_IOMONITOR_REST_HANDLER_H

#include "utils/network/tcp_connection_processor.h"
#include "core/resource/resource_fwd.h"

class QnIOMonitorConnectionProcessorPrivate;

class QnIOMonitorConnectionProcessor: public QnTCPConnectionProcessor
{
    Q_OBJECT
public:
    QnIOMonitorConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnIOMonitorConnectionProcessor();
protected:
    virtual void run() override;
    virtual void pleaseStop() override;
private slots:
    void at_cameraInput(
        const QnResourcePtr& resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp );

    void at_cameraOutput(
        const QnResourcePtr& resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp );
private:
    void onSomeBytesReadAsync( AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesRead );
private:
    Q_DECLARE_PRIVATE(QnIOMonitorConnectionProcessor);

    void sendMultipartData(QMutexLocker& lock);
};

#endif // QN_IOMONITOR_REST_HANDLER_H
