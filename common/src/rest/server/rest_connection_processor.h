#ifndef _REST_CONNECTION_PROCESSOR_H__
#define _REST_CONNECTION_PROCESSOR_H__

#include <QtCore/QVariantList>
#include "utils/network/tcp_connection_processor.h"
#include "request_handler.h"

class QnRestConnectionProcessorPrivate;

class QnRestConnectionProcessor: public QnTCPConnectionProcessor {
    Q_OBJECT
public:
    typedef QMap<QString, QnRestRequestHandlerPtr> Handlers;

    QnRestConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnRestConnectionProcessor();

    static void registerHandler(const QString& path, QnRestRequestHandler* handler);
    static QnRestRequestHandlerPtr findHandler(QString path);

protected:
    virtual void run() override;

private:
    static Handlers m_handlers;
    Q_DECLARE_PRIVATE(QnRestConnectionProcessor);
};

#endif // _REST_CONNECTION_PROCESSOR_H__
