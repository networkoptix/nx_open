#pragma once

#include "network/tcp_connection_processor.h"
#include "core/resource/resource_fwd.h"
#include "api/model/api_ioport_data.h"

class QnIOMonitorConnectionProcessorPrivate;

class QnIOMonitorConnectionProcessor: public QnTCPConnectionProcessor
{
    Q_OBJECT
public:
    QnIOMonitorConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner);
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
    void at_cameraInitDone(const QnResourcePtr& resource);
private:
    void onSomeBytesReadAsync(nx::network::AbstractStreamSocket* sock,
        SystemError::ErrorCode errorCode, size_t bytesRead);
    void onDataSent(SystemError::ErrorCode errorCode, size_t bytesSent);
    void addData(const QnIOStateDataList& value);
    void addData(const QnIOStateData& value);
    void addDataIfNeed(const QnIOStateData& value);
private:
    Q_DECLARE_PRIVATE(QnIOMonitorConnectionProcessor);

    void sendData();
    void sendNextMessage();
};
