#ifndef _REST_CONNECTION_PROCESSOR_H__
#define _REST_CONNECTION_PROCESSOR_H__

#include <QtCore/QVariantList>

#include <nx/network/http/httptypes.h>

#include "network/tcp_connection_processor.h"
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
    void registerHandler( const QString& path, QnRestRequestHandler* handler, Qn::GlobalPermission permissions = Qn::GlobalPermission::NoGlobalPermissions);
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
    void setAuthNotRequired(bool noAuth);

    QnUuid authUserId() const;
    void setAuthUserId(const QnUuid& authUserId);

    //!Rest handler can use following methods to access http request/response directly
    const nx_http::Request& request() const;
    nx_http::Response* response() const;
    QnTcpListener* owner() const;
protected:
    virtual void run() override;

private:
    bool m_noAuth;
    Q_DECLARE_PRIVATE(QnRestConnectionProcessor);
};

#endif // _REST_CONNECTION_PROCESSOR_H__
