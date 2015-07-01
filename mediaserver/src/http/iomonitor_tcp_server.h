#ifndef QN_IOMONITOR_REST_HANDLER_H
#define QN_IOMONITOR_REST_HANDLER_H

#include "utils/network/tcp_connection_processor.h"
#include "core/resource/resource_fwd.h"
#include "api/model/api_ioport_data.h"

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
    void at_cameraIOStateChanged(
        const QnResourcePtr& resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp );
    void at_cameraInitDone(const QnResourcePtr &resource);
private:
    void onSomeBytesReadAsync( AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesRead );
    void setData(QnIOStateDataList&& value);
    void addData(QnIOStateData&& value);
private:
    Q_DECLARE_PRIVATE(QnIOMonitorConnectionProcessor);

    void sendMultipartData();
};

#endif // QN_IOMONITOR_REST_HANDLER_H
