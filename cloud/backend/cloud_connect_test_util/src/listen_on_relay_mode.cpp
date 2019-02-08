#include "listen_on_relay_mode.h"

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/scope_guard.h>

#include "utils.h"

namespace nx {
namespace cctu {

namespace {

static void runServer(const ListenOnRelaySettings& settings)
{
    TestHttpServerOnProxy testHttpServerOnProxy(settings);

    std::cout << "Listening as " << settings.listeningPeerHostName()
        << " on relay " << settings.baseRelayUrl().toStdString() << std::endl;

    waitForExitCommand();
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

//-------------------------------------------------------------------------------------------------

TestHttpServerOnProxy::TestHttpServerOnProxy(
    const ListenOnRelaySettings& settings)
{
    using namespace std::placeholders;

    auto url = settings.baseRelayUrl();
    url.setUserName(settings.listeningPeerHostName().c_str());

    auto acceptor =
        std::make_unique<nx::network::cloud::relay::ConnectionAcceptor>(url);
    m_httpServer = std::make_unique<HttpServer>(std::move(acceptor));
}

} // namespace cctu
} // namespace nx
