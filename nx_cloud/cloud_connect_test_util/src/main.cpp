#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/socket_global.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/string.h>

#include "listen_mode.h"
#include "client_mode.h"

void printHelp()
{
    std::cout << "\n";
    nx::cctu::printListenOptions(&std::cout);
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

    // reading mode
    if (args.get("listen"))
        return nx::cctu::runInListenMode(args);

    if (args.get("connect"))
        return nx::cctu::runInConnectMode(args);

    if (args.get("http-client"))
        return nx::cctu::runInHttpClientMode(args);

    std::cerr<<"error. Unknown mode"<<std::endl;
    printHelp();
    return 1;
}

//--http-client --url=http://admin:admin@server1.ffc8e5a2-a173-4b3d-8627-6ab73d6b234d/api/gettime
//
//AK server:
//--http-client --url=http://admin:admin@47bf37a0-72a6-2890-b967-5da9c390d28a.c2cd3804-c66c-4ba4-900e-c27fd4d9180d/api/gettime
//
//AK home:
//--http-client --url=http://admin:admin@40bb3c45-6d46-4cee-bdfc-47986af175d1/api/moduleInformation
//
//LA server:
//--http-client --url=http://admin:admin@1af3ebeb-c327-3665-40f1-fa4dba0df78f.c2cd3804-c66c-4ba4-900e-c27fd4d9180d/api/gettime
//--http-client --url=http://admin:admin@becf0a3f-d101-44af-bdc3-13b2296604f0/api/gettime
//--connect --target=server1.c2cd3804-c66c-4ba4-900e-c27fd4d9180d --max-concurrent-connections=10
//--listen --server-id=server1 --cloud-credentials=c2cd3804-c66c-4ba4-900e-c27fd4d9180d:02e780b8-2dc3-4389-9af3-8170de591835

/** AK old PC mediator test:

--enforce-mediator=10.0.2.41:3345 --listen --ping --cloud-credentials=93e0467f-3145-41a8-8ebc-7f3c95e2ccf0:02e780b8-2dc3-4389-9af3-8170de591835 --server-id=xxx
--enforce-mediator=10.0.2.41:3345 --connect --ping --target=xxx.93e0467f-3145-41a8-8ebc-7f3c95e2ccf0 --bytes-to-receive=1m --total-connections=5 --max-concurrent-connections=5

--listen --local-address=0.0.0.0:5724 --udt
--connect --target=192.168.0.1:5724 --udt
--connect --target=192.168.0.1:5724 --udt --total-connections=1 --bytes-to-receive=100000000
*/
