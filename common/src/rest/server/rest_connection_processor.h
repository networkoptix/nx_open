#ifndef _REST_CONNECTION_PROCESSOR_H__
#define _REST_CONNECTION_PROCESSOR_H__

#include <QVariantList>
#include "utils/network/tcp_connection_processor.h"
#include "request_handler.h"

class QnRestConnectionProcessor: public QnTCPConnectionProcessor {
    Q_OBJECT
public:
    typedef QMap<QString, QnRestRequestHandlerPtr> Handlers;

    QnRestConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnRestConnectionProcessor();

    static void registerHandler(const QString& path, QnRestRequestHandler* handler);
    static QnRestRequestHandlerPtr findHandler(QString path);

protected:
    virtual void run() override;

private:
    static Handlers m_handlers;
    QN_DECLARE_PRIVATE_DERIVED(QnRestConnectionProcessor);
};

#endif // _REST_CONNECTION_PROCESSOR_H__
