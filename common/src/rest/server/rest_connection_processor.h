#ifndef _REST_CONNECTION_PROCESSOR_H__
#define _REST_CONNECTION_PROCESSOR_H__

#include <QtCore/QVariantList>

#include <nx/network/http/http_types.h>

#include "network/tcp_connection_processor.h"
#include "request_handler.h"
#include <core/resource_access/user_access_data.h>

class QnHttpConnectionListener;

class QnRestProcessorPool
{
public:
    typedef QMap<QString, QnRestRequestHandlerPtr> Handlers;
    typedef QMap<QString, QString> RedirectRules;

    QnRestProcessorPool();

    /*!
        Takes ownership of \a handler
    */
    void registerHandler( const QString& path, QnRestRequestHandler* handler, Qn::GlobalPermission permissions = Qn::NoGlobalPermissions);
    QnRestRequestHandlerPtr findHandler( QString path ) const;
    const Handlers& handlers() const;

    void registerRedirectRule( const QString& path, const QString& newPath );
    boost::optional<QString> getRedirectRule( const QString& path );

private:
    Handlers m_handlers;
    RedirectRules m_redirectRules;
};

class QnRestConnectionProcessor: public QnTCPConnectionProcessor
{
    Q_OBJECT

public:
    QnRestConnectionProcessor(
        QSharedPointer<nx::network::AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);
    virtual ~QnRestConnectionProcessor();
    void setAuthNotRequired(bool noAuth);

    Qn::UserAccessData accessRights() const;
    void setAccessRights(const Qn::UserAccessData& accessRights);

    //!Rest handler can use following methods to access http request/response directly
    const nx::network::http::Request& request() const;
    nx::network::http::Response* response() const;
    QnTcpListener* owner() const;

protected:
    virtual void run() override;

private:
    bool m_noAuth;
};

#endif // _REST_CONNECTION_PROCESSOR_H__
