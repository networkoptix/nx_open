#ifndef _REST_CONNECTION_PROCESSOR_H__
#define _REST_CONNECTION_PROCESSOR_H__

#include <QtCore/QVariantList>
#include "utils/network/tcp_connection_processor.h"
#include "request_handler.h"


class QnRestProcessorPool
{
public:
    typedef QMap<QString, QnRestRequestHandlerPtr> Handlers;

    /*!
        Takes ownership of \a handler
    */
    void registerHandler( const QString& path, QnRestRequestHandler* handler );
    QnRestRequestHandlerPtr findHandler( QString path ) const;
    const Handlers& handlers() const;

    static void initStaticInstance( QnRestProcessorPool* _instance );
    static QnRestProcessorPool* instance();

private:
    Handlers m_handlers;
};

class QnRestConnectionProcessorPrivate;

class QnRestConnectionProcessor: public QnTCPConnectionProcessor {
    Q_OBJECT

public:
    QnRestConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnRestConnectionProcessor();
    QnUuid authUserId() const;
protected:
    virtual void run() override;

private:
    Q_DECLARE_PRIVATE(QnRestConnectionProcessor);
};

#endif // _REST_CONNECTION_PROCESSOR_H__
