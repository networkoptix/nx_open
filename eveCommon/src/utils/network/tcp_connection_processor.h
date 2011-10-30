#ifndef __TCP_CONNECTION_PROCESSOR_H__
#define __TCP_CONNECTION_PROCESSOR_H__

#include <QMutex>

#include "utils/common/longrunnable.h"
#include "utils/network/socket.h"
#include "utils/common/base.h"

class QnTcpListener;

class QnTCPConnectionProcessor: public CLLongRunnable
{
public:
    QnTCPConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnTCPConnectionProcessor();

protected:
    virtual void parseRequest();
    QMutex& getSockMutex();
    QString extractPath() const;
    void sendData(const char* data, int size);
    inline void sendData(const QByteArray& data) { sendData(data.constData(), data.size()); }
    void sendResponse(const QByteArray& transport, int code, const QByteArray& contentType);
    bool isFullMessage();
    QString codeToMessage(int code);

    QN_DECLARE_PRIVATE(QnTCPConnectionProcessor);
    QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* d_ptr, TCPSocket* socket, QnTcpListener* owner);
};

#endif // __TCP_CONNECTION_PROCESSOR_H__
