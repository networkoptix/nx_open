#ifndef _REST_CONNECTION_PROCESSOR_H__
#define _REST_CONNECTION_PROCESSOR_H__

#include <QtCore/QVariantList>

#include <utils/network/http/httptypes.h>
#include "utils/network/tcp_connection_processor.h"
#include "request_handler.h"


class QnRestProcessorPool
:
    public Singleton<QnRestProcessorPool>
{
public:
    typedef QMap<QString, QnRestRequestHandlerPtr> Handlers;

    /*!
        Takes ownership of \a handler
    */
    void registerHandler( const QString& path, QnRestRequestHandler* handler );
    QnRestRequestHandlerPtr findHandler( QString path ) const;
    const Handlers& handlers() const;

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

    //!Rest handler can use following methods to access http request/response directly
    const nx_http::Request& request() const;
    nx_http::Response* response() const;

protected:
    virtual void run() override;

private:
    Q_DECLARE_PRIVATE(QnRestConnectionProcessor);
};

#endif // _REST_CONNECTION_PROCESSOR_H__
