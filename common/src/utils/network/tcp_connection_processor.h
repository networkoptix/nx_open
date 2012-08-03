#ifndef __TCP_CONNECTION_PROCESSOR_H__
#define __TCP_CONNECTION_PROCESSOR_H__

#include <QMutex>

#include "utils/common/longrunnable.h"
#include "utils/network/socket.h"
#include "utils/common/pimpl.h"

class QnTcpListener;

class QnTCPConnectionProcessor: public QnLongRunnable {
    Q_OBJECT;

public:
    QnTCPConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnTCPConnectionProcessor();

    /**
     * Check for request or response is completed: finished with /r/n/r/n or contains full content len data
     */
    static int isFullMessage(const QByteArray& message);

    int getSocketTimeout();

protected:
    virtual void pleaseStop();
    virtual void parseRequest();
    QString extractPath() const;
    void sendData(const char* data, int size);
    inline void sendData(const QByteArray& data) { sendData(data.constData(), data.size()); }

    void bufferData(const char* data, int size);
    inline void bufferData(const QByteArray& data) { bufferData(data.constData(), data.size()); }
    void sendBuffer();
    void clearBuffer();

    void sendResponse(const QByteArray& transport, int code, const QByteArray& contentType);
    QString codeToMessage(int code);

    QN_DECLARE_PRIVATE(QnTCPConnectionProcessor);
    QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* d_ptr, TCPSocket* socket, QnTcpListener* owner);
};

#endif // __TCP_CONNECTION_PROCESSOR_H__
