#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

#include <nx/network/socket_global.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/string.h>

#include "listen_mode.h"
#include "client_mode.h"

void printHelp(int /*argc*/, char* /*argv*/[])
{
    std::cout << "\n";
    nx::cctu::printListenOptions(&std::cout);
    std::cout << "\n";
    nx::cctu::printConnectOptions(&std::cout);
    std::cout << "\n";
    nx::cctu::printHttpClientOptions(&std::cout);

    std::cout <<
        "\n"
        "Common options:\n"
        "  --enforce-mediator={endpoint}    Enforces custom mediator address\n"
        "  --log-level={level}              Log level to console"
        "\n"
        << std::endl;
}

int main(int argc, char* argv[])
{
    std::multimap<QString, QString> args;
    parseCmdArgs(argc, argv, &args);
    if (args.find("help") != args.end())
    {
        printHelp(argc, argv);
        return 0;
    }

    nx::network::SocketGlobals::InitGuard socketGlobalsGuard;

    // common options
    const auto mediatorIt = args.find("enforce-mediator");
    if (mediatorIt != args.end())
    {
        nx::network::SocketGlobals::mediatorConnector().
            mockupAddress(mediatorIt->second);
    }
    const auto logLevelIt = args.find("log-level");
    if (logLevelIt != args.end())
        QnLog::initLog(logLevelIt->second);

    // reading mode
    if (args.find("listen") != args.end())
        return nx::cctu::runInListenMode(args);

    if (args.find("connect") != args.end())
        return nx::cctu::runInConnectMode(args);

    if (args.find("http-client") != args.end())
        return nx::cctu::runInHttpClientMode(args);

    std::cerr<<"error. Unknown mode"<<std::endl;
    printHelp(argc, argv);

    return 0;
}

//--http-client --url=http://admin:admin@server1.ffc8e5a2-a173-4b3d-8627-6ab73d6b234d/api/gettime
//AK server:
//--http-client --url=http://admin:admin@47bf37a0-72a6-2890-b967-5da9c390d28a.c2cd3804-c66c-4ba4-900e-c27fd4d9180d/api/gettime
//LA server:
//--http-client --url=http://admin:admin@1af3ebeb-c327-3665-40f1-fa4dba0df78f.c2cd3804-c66c-4ba4-900e-c27fd4d9180d/api/gettime
//--http-client --url=http://admin:admin@c2cd3804-c66c-4ba4-900e-c27fd4d9180d/api/gettime
//--connect --target=server1.c2cd3804-c66c-4ba4-900e-c27fd4d9180d --max-concurrent-connections=10
//--listen --server-id=server1 --cloud-credentials=c2cd3804-c66c-4ba4-900e-c27fd4d9180d:02e780b8-2dc3-4389-9af3-8170de591835

/** AK old PC mediator test:

--enforce-mediator=10.0.2.41:3345 --listen --echo --cloud-credentials=93e0467f-3145-41a8-8ebc-7f3c95e2ccf0:02e780b8-2dc3-4389-9af3-8170de591835 --server-id=xxx
--enforce-mediator=10.0.2.41:3345 --connect --echo --target=xxx.93e0467f-3145-41a8-8ebc-7f3c95e2ccf0 --bytes-to-receive=1m --total-connections=5 --max-concurrent-connections=5

--listen --local-address=0.0.0.0:5724 --udt
--connect --target=192.168.0.1:5724 --udt
--connect --target=192.168.0.1:5724 --udt --total-connections=1 --bytes-to-receive=100000000
*/
