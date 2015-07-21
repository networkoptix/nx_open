#ifndef QN_FILE_CONNECTION_PROCESSOR_H
#define QN_FILE_CONNECTION_PROCESSOR_H

#include "utils/network/tcp_connection_processor.h"

class QnFileConnectionProcessorPrivate;

class QnFileConnectionProcessor: public QnTCPConnectionProcessor {
    Q_OBJECT

public:
    QnFileConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnFileConnectionProcessor();

    static QByteArray readStaticFile(const QString& path);
protected:
    virtual void run() override;

private:
    Q_DECLARE_PRIVATE(QnFileConnectionProcessor);
};

#endif // QN_FILE_REST_HANDLER_H
