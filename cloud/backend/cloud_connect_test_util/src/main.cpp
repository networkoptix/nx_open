#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/socket_global.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/string.h>

#include "client_mode.h"
#include "listen_mode.h"
#include "listen_on_relay_mode.h"
#include "relay_load_test.h"
#include "test_relay.h"

void printHelp()
{
    std::cout << "\n";
    nx::cctu::printListenOptions(&std::cout);
    std::cout << "\n";
    nx::cctu::printListenOnRelayOptions(&std::cout);
    std::cout << "\n";
    nx::cctu::printTestRelayOptions(&std::cout);
    std::cout << "\n";
    nx::cctu::RelayLoadTest::printOptions(&std::cout);
    std::cout << "\n";
    nx::cctu::printConnectOptions(&std::cout);
    std::cout << "\n";
    nx::cctu::printHttpClientOptions(&std::cout);

    std::cout <<
        std::endl <<
        "Common options:" << std::endl <<
        "  --log-level={level}              Log level to console" << std::endl <<
        "  --rw-timeout={time}              Socket send/recv timeouts" << std::endl;

    nx::network::SocketGlobals::printArgumentsHelp(&std::cout);

    std::cout <<
        std::endl <<
        std::endl;
}

int main(int argc, const char* argv[])
{
    using namespace nx::network;

    nx::utils::ArgumentParser args(argc, argv);
    if (args.get("help"))
    {
        printHelp();
        return 0;
    }

    SocketGlobals::InitGuard socketGlobalsGuard;

    // common options
    nx::utils::log::initializeGlobally(args);
    SocketGlobals::applyArguments(args);

    try
    {
        // reading mode
        if (args.get("listen"))
            return nx::cctu::runInListenMode(args);

        if (args.get("listen-on-relay"))
            return nx::cctu::runInListenOnRelayMode(args);

        if (args.get(nx::cctu::RelayLoadTest::kModeName))
            return nx::cctu::RelayLoadTest::run(args);

        if (args.get("test-relay"))
            return nx::cctu::testRelay(args);

        if (args.get("connect"))
            return nx::cctu::runInConnectMode(args);

        if (args.get("http-client"))
            return nx::cctu::runInHttpClientMode(args);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 2;
    }

    std::cerr<<"error. Unknown mode"<<std::endl;
    printHelp();
    return 1;
}
