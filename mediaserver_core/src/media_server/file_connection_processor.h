#pragma once

#include "network/tcp_connection_processor.h"

class QnFileConnectionProcessorPrivate;
typedef std::unique_ptr<QIODevice> QIODevicePtr;

class QnFileConnectionProcessor: public QnTCPConnectionProcessor {
    Q_OBJECT

public:
    static QString externalPackagePath();

    QnFileConnectionProcessor(std::unique_ptr<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnFileConnectionProcessor();

    static QByteArray readStaticFile(const QString& path);

protected:
    virtual void run() override;

private:
    bool loadFile(
        const QString& path,
        QDateTime* outLastModified,
        QByteArray* outData);
    QByteArray compressMessageBody(const QByteArray& contentType);

private:
    Q_DECLARE_PRIVATE(QnFileConnectionProcessor);
};
