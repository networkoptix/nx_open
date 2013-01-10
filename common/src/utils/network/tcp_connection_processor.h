#ifndef __TCP_CONNECTION_PROCESSOR_H__
#define __TCP_CONNECTION_PROCESSOR_H__

#include <QMutex>
#include <QUrl>

#include "utils/common/long_runnable.h"
#include "utils/network/socket.h"
#include "utils/common/pimpl.h"
#include "utils/common/byte_array.h"

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

    bool sendChunk(const QnByteArray& chunk);

    void execute(QMutex& mutex);
    virtual void pleaseStop();

    QString remoteHostAddress() const;

protected:
    virtual void parseRequest();
    QString extractPath() const;
    static QString extractPath(const QString& fullUrl);
    /*!
        \return Number of bytes actually sent, -1 in case of error
        \note If managed to send something and then error occured, then number of actually sent bytes is returned
    */
    int sendData(const char* data, int size);
    inline int sendData(const QByteArray& data) { return sendData(data.constData(), data.size()); }

    //QnByteArray& getSendBuffer();
    //void bufferData(const char* data, int size);
    //inline void bufferData(const QByteArray& data) { bufferData(data.constData(), data.size()); }
    void sendBuffer(const QnByteArray& sendBuffer);
    //void clearBuffer();

    void sendResponse(const QByteArray& transport, int code, const QByteArray& contentType, bool displayDebug = false);
    QString codeToMessage(int code);

    void copyClientRequestTo(QnTCPConnectionProcessor& other);
    /*!
        \return Number of bytes read. 0 if connection has been closed. -1 in case of error
        \note Usage of this method MUST NOT be mixed with usage of \a readRequest / \a parseRequest
    */
    int readSocket( quint8* buffer, int bufSize );
    bool readRequest();
    QUrl getDecodedUrl() const;

    QN_DECLARE_PRIVATE(QnTCPConnectionProcessor);
    QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* d_ptr, TCPSocket* socket, QnTcpListener* owner);
};

#endif // __TCP_CONNECTION_PROCESSOR_H__
