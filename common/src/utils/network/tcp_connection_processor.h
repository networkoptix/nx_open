#ifndef __TCP_CONNECTION_PROCESSOR_H__
#define __TCP_CONNECTION_PROCESSOR_H__

#include <QtCore/QMutex>
#include <QtCore/QUrl>

#include "utils/common/long_runnable.h"
#include "utils/network/socket.h"
#include "utils/common/byte_array.h"

class QnTcpListener;
class QnTCPConnectionProcessorPrivate;

class QnTCPConnectionProcessor: public QnLongRunnable {
    Q_OBJECT;

public:
    QnTCPConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket);
    virtual ~QnTCPConnectionProcessor();

    /**
     * Check for request or response is completed: finished with /r/n/r/n or contains full content len data
     */
    static int isFullMessage(const QByteArray& message);

    int getSocketTimeout();

    bool sendChunk(const QnByteArray& chunk);
    bool sendChunk(const QByteArray& chunk);
    bool sendChunk(const char* data, int size);

    void execute(QMutex& mutex);
    virtual void pleaseStop();
    QSharedPointer<AbstractStreamSocket> socket() const;
    QUrl getDecodedUrl() const;

    bool sendBuffer(const QnByteArray& sendBuffer);
    bool sendBuffer(const QByteArray& sendBuffer);

    bool readRequest();
    virtual void parseRequest();

    virtual bool isTakeSockOwnership() const { return false; }
    void releaseSocket();
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
    /*!
        \return Number of bytes read. 0 if connection has been closed. -1 in case of error
        \note Usage of this method MUST NOT be mixed with usage of \a readRequest / \a parseRequest
    */
    int readSocket( quint8* buffer, int bufSize );

    QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* d_ptr, QSharedPointer<AbstractStreamSocket> socket);

private:
    bool sendData(const char* data, int size);
    inline bool sendData(const QByteArray& data) { return sendData(data.constData(), data.size()); }
protected:
    Q_DECLARE_PRIVATE(QnTCPConnectionProcessor);

    QnTCPConnectionProcessorPrivate *d_ptr;
};

#endif // __TCP_CONNECTION_PROCESSOR_H__
