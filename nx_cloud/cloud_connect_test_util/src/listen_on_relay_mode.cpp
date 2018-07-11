#include "listen_on_relay_mode.h"

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/scope_guard.h>

namespace nx {
namespace cctu {

namespace {

static void runServer(const ListenOnRelaySettings& settings)
{
    TestHttpServerOnProxy testHttpServerOnProxy(settings);

    std::cout << "Listening as " << settings.listeningPeerHostName()
        << " on relay " << settings.baseRelayUrl().toStdString() << std::endl;

    for (;;)
    {
        std::string command;
        std::cin >> command;
        if (command == "exit")
            return;
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

ListenOnRelaySettings::ListenOnRelaySettings(
    const nx::utils::ArgumentParser& args)
    :
    m_listeningPeerHostName(
        QnUuid::createUuid().toSimpleByteArray().toStdString())
{
    if (auto relayUrl = args.get("relay-url"))
        m_baseRelayUrl = nx::utils::Url(*relayUrl);
    else
        throw std::invalid_argument("Missing required attribute \"relay-url\"");

    if (auto hostName = args.get("listening-peer-host-name"))
        m_listeningPeerHostName = hostName->toStdString();
}

nx::utils::Url ListenOnRelaySettings::baseRelayUrl() const
{
    return m_baseRelayUrl;
}

std::string ListenOnRelaySettings::listeningPeerHostName() const
{
    return m_listeningPeerHostName;
}

//-------------------------------------------------------------------------------------------------

void printListenOnRelayOptions(std::ostream* outStream)
{
    *outStream <<
        "Listen on relay mode:\n"
        "On success, service will be available for requests as http://peer-hostname.relayhost/\n"
        "  --listen-on-relay                Enable mode\n"
        "  --relay-url={URL}                E.g., http://relay.vmsproxy.com/\n"
        "  --listening-peer-host-name={globally unique hostname}. Optional\n";
}

int runInListenOnRelayMode(const nx::utils::ArgumentParser& args)
{
    ListenOnRelaySettings settings(args);
    runServer(settings);

    return 0;
}

TestHttpServerOnProxy::TestHttpServerOnProxy(
    const ListenOnRelaySettings& settings)
{
    using namespace std::placeholders;

    auto url = settings.baseRelayUrl();
    url.setUserName(settings.listeningPeerHostName().c_str());

    auto acceptor =
        std::make_unique<nx::network::cloud::relay::ConnectionAcceptor>(url);
    m_httpServer = std::make_unique<nx::network::http::TestHttpServer>(
        std::move(acceptor));
    m_httpServer->registerRequestProcessorFunc(
        nx::network::http::kAnyPath,
        std::bind(&TestHttpServerOnProxy::httpRequestHandler, this,
            _1, _2, _3, _4, _5));
    m_httpServer->server().start();
}

void TestHttpServerOnProxy::httpRequestHandler(
    nx::network::http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx::network::http::Request request,
    nx::network::http::Response* const /*response*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    auto responseMessage = std::make_unique<nx::network::http::BufferSource>(
        "text/plain",
        lm("Hello from %1 %2\r\n")
        .args(request.requestLine.method, request.requestLine.url.path()).toUtf8());

    std::cout << request.requestLine.method.toStdString() << " "
        << request.requestLine.url.path().toStdString()
        << std::endl;

    completionHandler(nx::network::http::RequestResult(
        nx::network::http::StatusCode::ok,
        std::move(responseMessage)));
}

} // namespace cctu
} // namespace nx
