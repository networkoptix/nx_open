/**********************************************************
* mar 29, 2016
* a.kolesnikov
***********************************************************/

#include "listen_mode.h"

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/test_support/socket_test_helper.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/string.h>


int runInListenMode(const std::multimap<QString, QString>& args)
{
    using namespace nx::network;

    auto transmissionMode = nx::network::test::TestTransmissionMode::spam;
    if (args.find("echo") != args.end())
        transmissionMode = nx::network::test::TestTransmissionMode::echo;

    test::RandomDataTcpServer server(
        test::TestTrafficLimitType::none, 0, transmissionMode);

    auto credentialsIter = args.find("cloud-credentials");
    auto localAddressIter = args.find("local-address");
    QStringList cloudCredentials;
    std::unique_ptr<AbstractStreamServerSocket> serverSocket;
    if (credentialsIter != args.end())
    {
        cloudCredentials = credentialsIter->second.split(":");
        if (cloudCredentials.size() != 2)
        {
            std::cerr << "error. Parameter cloud-credentials MUST have format system_id:authentication_key" << std::endl;
            return 1;
        }

        QString serverId = generateRandomName(7);
        readArg(args, "server-id", &serverId);

        SocketGlobals::mediatorConnector().setSystemCredentials(
            nx::hpm::api::SystemCredentials(
                cloudCredentials[0].toUtf8(),
                serverId.toUtf8(),
                cloudCredentials[1].toUtf8()));
        SocketGlobals::mediatorConnector().enable(true);

        auto cloudServerSocket = std::make_unique<cloud::CloudServerSocket>(
            SocketGlobals::mediatorConnector().systemConnection());
        if (!cloudServerSocket->registerOnMediatorSync())
        {
            std::cerr << "error. Failed to listen on mediator. Reason: " <<
                SystemError::getLastOSErrorText().toStdString() << std::endl;
            return 1;
        }
        serverSocket = std::move(cloudServerSocket);
        std::cout << "listening on mediator. Address "
            << serverId.toStdString() << "." << cloudCredentials[0].toStdString()
            << std::endl;
    }
    else if (localAddressIter != args.end())
    {
        const bool isUdt = args.find("udt") != args.end();

        if (isUdt)
            serverSocket = std::make_unique<UdtStreamServerSocket>();
        else
            serverSocket = std::make_unique<TCPServerSocket>();
        server.setLocalAddress(SocketAddress(localAddressIter->second));
        std::cout << "listening on local "<<(isUdt ? "udt" : "tcp")<<" address "
            << localAddressIter->second.toStdString() << std::endl;
    }
    else
    {
        std::cerr << "error. You have to specifiy --cloud-credentials or --local-address " << std::endl;
        return 1;
    }

    server.setServerSocket(std::move(serverSocket));
    if (!server.start())
    {
        auto osErrorText = SystemError::getLastOSErrorText().toStdString();
        std::cerr << "error. Failed to start accepting connections. Reason: "
            << osErrorText << std::endl;
        return 1;
    }

#if 0
    const auto showHelp = []()
    {
        std::cout <<
            "Commands: \n"
            "    help       Print this message\n"
            "    status     Print status line\n"
            "    exit       Exit\n";
    };

    showHelp();
    std::cout << ">> ";
    for (std::string s; getline(std::cin, s); std::cout << ">> ")
    {
        if (s == "help")
            showHelp();
        else
            if (s == "st" || s == "status")
                std::cout << server.statusLine().toStdString() << std::endl;
            else
                if (s == "exit")
                    break;
    }
    const int result = 0;
#else
    const int result = printStatsAndWaitForCompletion(
        &server,
        nx::utils::MoveOnlyFunc<bool()>());
#endif
    server.pleaseStopSync();
    return result;
}

void printListenOptions(std::ostream* const outStream)
{
    *outStream<<
    "Listen mode (can listen on local or cloud address):\n"
    "  --listen                         Enable listen mode\n"
    "  --echo                           Makes server to mirror data instead of spaming\n"
    "  --cloud-credentials={system_id}:{authentication_key}\n"
    "                                   Specify credentials to use to connect to mediator\n"
    "  --server-id={server_id}          Id used when registering on mediator\n"
    "  --local-address={ip:port}        Local address to listen\n"
    "  --udt                            Use udt instead of tcp. Only if listening local address\n";
}

int printStatsAndWaitForCompletion(
    nx::network::test::ConnectionPool* const connectionPool,
    nx::utils::MoveOnlyFunc<bool()> interruptCondition)
{
    using nx::network::test::ConnectionTestStatistics;
    using namespace std::chrono;

    constexpr const auto kUpdateStatisticsInterval = std::chrono::seconds(1);
    constexpr const auto kStatisticsResetInterval = std::chrono::seconds(10);
    constexpr const auto zeroStatistics = ConnectionTestStatistics{ 0, 0, 0, 0 };

    std::cout << "\nUsage statistics:" << std::endl;
    ConnectionTestStatistics prevStatistics{ -1, -1, -1, -1 };
    ConnectionTestStatistics baseStatisticsData = zeroStatistics;
    boost::optional<steady_clock::time_point> sameStatisticsInterval;
    std::string prevStatToDisplayStr;
    for (;;)
    {
        if (interruptCondition && interruptCondition())
            return 0;

        const auto data = connectionPool->statistics();
        auto statToDisplay = data - baseStatisticsData;

        if (statToDisplay != zeroStatistics &&
            data == prevStatistics)
        {
            if (!sameStatisticsInterval)
            {
                sameStatisticsInterval = steady_clock::now();
            }
            else
            {
                const auto timePassed = steady_clock::now() - sameStatisticsInterval.get();
                if (timePassed > kStatisticsResetInterval)
                {
                    std::cout << "\nNo activity for "
                        << (duration_cast<seconds>(timePassed)).count() << "s. "
                        << "Resetting statistics...\n" << std::endl;;
                    //resetting statistics
                    baseStatisticsData = data;
                    prevStatistics = ConnectionTestStatistics{
                        (uint64_t)-1, (uint64_t)-1, (size_t)-1, (size_t)-1 };
                    statToDisplay = zeroStatistics;
                    sameStatisticsInterval.reset();
                }
            }
        }
        else
        {
            sameStatisticsInterval.reset();
        }

        prevStatistics = data;
        const auto statToDisplayStr = toString(statToDisplay).toStdString();

        std::cout << '\r';
        std::cout << statToDisplayStr;
        if (prevStatToDisplayStr.size() > statToDisplayStr.size())
            std::cout << std::string(prevStatToDisplayStr.size() - statToDisplayStr.size(), ' ');
        std::cout << std::flush;
        prevStatToDisplayStr = statToDisplayStr;
        std::this_thread::sleep_for(kUpdateStatisticsInterval);
    }

    return 0;
}
