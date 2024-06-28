// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMimeDatabase>
#include <QtCore/QPointer>
#include <QtCore/QUrlQuery>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlEngine>

#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/buffer_source.h>
#include <nx/vms/client/core/application_context.h>

#include "js_utils.h"
#include "testkit.h"

namespace nx::vms::client::core::testkit {

using namespace nx::network::http;

struct HttpServer::Private
{
    std::unique_ptr<nx::network::http::MessageDispatcher> dispatcher;
    std::unique_ptr<nx::network::http::HttpStreamSocketServer> server;

    QJsonObject evaluate(const QString& source)
    {
        auto engine = appContext()->qmlEngine();
        if (!engine)
            return makeErrorResponse("Missing QJSEngine");

        // Execute the provided script.
        // Event loop may execute here as a result of script actions!
        const QJSValue result = engine->evaluate(source);
        return makeResponse(result);
    }

    void processRequest(RequestContext requestContext, RequestProcessedHandler completionHandler)
    {
        static QMimeDatabase mimeDatabase;

        QUrlQuery query(requestContext.request.messageBody.toByteArray());

        if (const auto path = requestContext.request.requestLine.url.path();
            requestContext.request.requestLine.method == Method::get
            && (path == "/screenshot"
            || path == "/screenshot.jpg"
            || path == "/screenshot.jpeg"
            || path == "/screenshot.png"))
        {
            QByteArray data = TestKit::screenshot(path.endsWith(".png") ? "PNG" : "JPG");

            auto responseMessage = std::make_unique<nx::network::http::BufferSource>(
                mimeDatabase.mimeTypeForData(data).name().toStdString(),
                std::move(data));

            completionHandler(nx::network::http::RequestResult(
                nx::network::http::StatusCode::ok,
                std::move(responseMessage)));
            return;
        }

        QJsonObject response;

        if (query.queryItemValue("command") == "execute")
        {
            const QString source = query.queryItemValue("source", QUrl::FullyDecoded);
            response = evaluate(source);
        }

        QJsonObject obj;
        obj["line"] = QString::fromStdString(requestContext.request.requestLine.toString());

        auto responseMessage = std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            QJsonDocument(response).toJson());

        completionHandler(nx::network::http::RequestResult(
            nx::network::http::StatusCode::ok,
            std::move(responseMessage)));
    }
};

HttpServer::HttpServer(int port): d(new Private)
{
    d->dispatcher = std::make_unique<nx::network::http::MessageDispatcher>();
    d->server = std::make_unique<nx::network::http::HttpStreamSocketServer>(d->dispatcher.get());

    auto func =
        [self=QPointer(this)]
        (RequestContext requestContext, RequestProcessedHandler completionHandler) mutable
        {
            auto callback = [
                    self=self,
                    requestContext=std::move(requestContext),
                    completionHandler=std::move(completionHandler)]() mutable
                {
                    if (!self)
                        return;

                    self->d->processRequest(
                        std::move(requestContext),
                        std::move(completionHandler));
                };

            QMetaObject::invokeMethod(qApp, std::move(callback), Qt::QueuedConnection);
        };

    d->dispatcher->registerRequestProcessorFunc(Method::get, nx::network::http::kAnyPath, func);
    d->dispatcher->registerRequestProcessorFunc(Method::post, nx::network::http::kAnyPath, func);

    if (!d->server->bind(network::SocketAddress(network::HostAddress::anyHost, port)))
        throw std::runtime_error("Unable to start HTTP server: bind returned false");

    if (!d->server->listen())
        throw std::runtime_error("Unable to start HTTP server: listen returned false");
}

HttpServer::~HttpServer()
{}

} // namespace nx::vms::client::core::testkit
