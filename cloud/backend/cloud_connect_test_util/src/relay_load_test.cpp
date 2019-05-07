#include "relay_load_test.h"

#include "listen_on_relay_mode.h"

#include "utils.h"

namespace nx::cctu {

RelayLoadTest::Settings::Settings(const nx::utils::ArgumentParser& args)
{
    if (auto relayUrlValue = args.get("relay-url"))
        relayUrl = relayUrlValue->toStdString();
    else
        throw std::invalid_argument("Missing required argument \"relay-url\"");

    if (auto countValue = args.get<int>("count"))
        count = *countValue;
    else
        throw std::invalid_argument("Missing required argument \"count\"");
}

void RelayLoadTest::printOptions(std::ostream* output)
{
    *output <<
        "Relay load test:\n"
        "  --relay-load-test                Enable mode\n"
        "  --relay-url={URL}                E.g., http://relay.vmsproxy.com/\n"
        "  --count={number of servers to emulate}.  Required\n";
}

int RelayLoadTest::run(const nx::utils::ArgumentParser& args)
{
    Settings settings(args);
    std::vector<std::unique_ptr<TestHttpServerOnProxy>> servers;
    servers.reserve(settings.count);

    for (unsigned int i = 0; i < settings.count; ++i)
    {
        ListenOnRelaySettings serverSettings;
        serverSettings.baseRelayUrl = settings.relayUrl;
        serverSettings.listeningPeerHostName =
            "loadtest-" + std::to_string(i) + "-" +
            QnUuid::createUuid().toSimpleString().toStdString();

        servers.push_back(std::make_unique<TestHttpServerOnProxy>(serverSettings));
    }

    std::cout << "Started as " << servers.size() << " servers "
        << "on relay " << settings.relayUrl << std::endl;

    waitForExitCommand();
    return 0;
}

} // namespace nx::cctu
