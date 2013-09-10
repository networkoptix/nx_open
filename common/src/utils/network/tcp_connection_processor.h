#ifndef __TCP_CONNECTION_PROCESSOR_H__
#define __TCP_CONNECTION_PROCESSOR_H__

#include <QtCore/QMutex>
#include <QtCore/QUrl>

#include "utils/common/long_runnable.h"
#include "utils/network/socket.h"
#include "utils/common/byte_array.h"
#include <openssl/ssl.h>

class QnTcpListener;
class QnTCPConnectionProcessorPrivate;

class QnTCPConnectionProcessor: public QnLongRunnable {
    Q_OBJECT;

public:
    QnTCPConnectionProcessor(AbstractStreamSocket* socket, SSL_CTX* sslContext);
    virtual ~QnTCPConnectionProcessor();

    /**
     * Check for request or response is completed: finished with /r/n/r/n or contains full content len data
     */
    static int isFullMessage(const QByteArray& message);

    int getSocketTimeout();

    bool sendChunk(const QnByteArray& chunk);

    void execute(QMutex& mutex);
    virtual void pleaseStop();

    //!Returns SSL*. including ssl.h here causes numerous compilation problems
    void* ssl() const;
    AbstractStreamSocket* socket() const;
    QUrl getDecodedUrl() const;

    bool sendBuffer(const QnByteArray& sendBuffer);
    bool sendBuffer(const QByteArray& sendBuffer);

    bool readRequest();
    virtual void parseRequest();
protected:
    QString extractPath() const;
    static QString extractPath(const QString& fullUrl);

    //QnByteArray& getSendBuffer();
    //void bufferData(const char* data, int size);
    //inline void bufferData(const QByteArray& data) { bufferData(data.constData(), data.size()); }
    //void clearBuffer();

    void sendResponse(const QByteArray& transport, int code, const QByteArray& contentType, const QByteArray& contentEncoding = QByteArray(), bool displayDebug = false);
    QString codeToMessage(int code);

    void copyClientRequestTo(QnTCPConnectionProcessor& other);

    QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* d_ptr, AbstractStreamSocket* socket, void* _sslContext);
private:
    bool sendData(const char* data, int size);
    inline bool sendData(const QByteArray& data) { return sendData(data.constData(), data.size()); }
protected:
    Q_DECLARE_PRIVATE(QnTCPConnectionProcessor);

    QnTCPConnectionProcessorPrivate *d_ptr;
};

#endif // __TCP_CONNECTION_PROCESSOR_H__
