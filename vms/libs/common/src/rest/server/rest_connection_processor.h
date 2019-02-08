#pragma once

#include <QtCore/QVariantList>

#include <nx/network/http/http_types.h>

#include "network/tcp_connection_processor.h"
#include "request_handler.h"
#include <core/resource_access/user_access_data.h>

class QnHttpConnectionListener;

class QnRestProcessorPool
{
    using HandlersByPath = QMap<QString, QnRestRequestHandlerPtr>;
    using Handlers = std::map<nx::network::http::Method::ValueType, HandlersByPath>;
    using RedirectRules = QMap<QString, QString>;

public:
    static const nx::network::http::Method::ValueType kAnyHttpMethod;
    static const QString kAnyPath;

    void registerHandler(
        const QString& path,
        QnRestRequestHandler* handler,
        GlobalPermission permission = GlobalPermission::none);

    void registerHandler(
        nx::network::http::Method::ValueType httpMethod,
        const QString& path,
        QnRestRequestHandler* handler,
        GlobalPermission permission = GlobalPermission::none);

    QnRestRequestHandlerPtr findHandler(
        const nx::network::http::Method::ValueType& httpMethod,
        const QString& path) const;

    void registerRedirectRule(const QString& path, const QString& newPath);
    boost::optional<QString> getRedirectRule(const QString& path);

private:
    QnRestRequestHandlerPtr findHandlerForSpecificMethod(
        const nx::network::http::Method::ValueType& httpMethod,
        const QString& path) const;

    QnRestRequestHandlerPtr findHandlerByPath(
        const HandlersByPath& handlersByPath,
        const QString& path) const;

private:
    /**
     * NOTE: Empty method name means any method.
     */
    Handlers m_handlers;
    RedirectRules m_redirectRules;
};

//-------------------------------------------------------------------------------------------------

class QnRestConnectionProcessor: public QnTCPConnectionProcessor
{
    Q_OBJECT

public:
    QnRestConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);
    virtual ~QnRestConnectionProcessor();
    void setAuthNotRequired(bool noAuth);

    Qn::UserAccessData accessRights() const;
    void setAccessRights(const Qn::UserAccessData& accessRights);

    //!Rest handler can use following methods to access http request/response directly
    const nx::network::http::Request& request() const;
    nx::network::http::Response* response() const;

protected:
    virtual void run() override;

private:
    bool m_noAuth;
};
