#include "listen_mode.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/ssl_socket.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/thread/barrier_handler.h>

#include "dynamic_statistics.h"

namespace nx {
namespace cctu {

static const int kMaxAddressesToPrint = 10;

void printListenOptions(std::ostream* const outStream)
{
    *outStream<<
    "Listen mode (can listen on local or cloud address):\n"
    "  --listen                     Enable listen mode\n"
    "  --ping                       Makes server to mirror data instead of spaming\n"
    "  --cloud-credentials={system_id}:{authentication_key}\n"
    "                               Specify credentials to use to connect to mediator\n"
    "  --server-id={server_id}      Id used when registering on mediator (optional)\n"
    "  --server-count=N             Random generated server Ids (to emulate several servers)\n"
    "  --local-address={ip:port}    Local address to listen (default: 127.0.0.1:0)\n"
    "  --forward-address[=address]  Bind local address to mediator (default is local-address)\n"
    "  --udt                        Use udt instead of tcp. Only if listening local address\n"
    "  --ssl                        Uses SSL on top of server socket type\n";
}

class CloudServerSocketGenerator
{
public:
    std::unique_ptr<nx::network::AbstractStreamServerSocket> make(
        String systemId, String authKey, String serverId)
    {
        socketContexts.push_back(SocketContext(
            std::move(systemId), std::move(authKey), std::move(serverId)));

        auto& currentContext = socketContexts.back();
        auto socket = std::make_unique<network::cloud::CloudServerSocket>(
            currentContext.mediatorConnector.get());

        if (connectionMethonds)
            socket->setSupportedConnectionMethods(*connectionMethonds);

        currentContext.socket = socket.get();
        return std::move(socket);
    }

    struct SocketContext
    {
        std::unique_ptr<hpm::api::MediatorConnector> mediatorConnector;
        std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection;
        network::cloud::CloudServerSocket* socket;
        QString listeningAddress;

        SocketContext(const SocketContext&) = delete;
        SocketContext(SocketContext&&) = default;
        SocketContext& operator=(const SocketContext&) = delete;
        SocketContext& operator=(SocketContext&&) = default;

        SocketContext(String systemId, String authKey, String serverId)
        :
            mediatorConnector(new hpm::api::MediatorConnector),
            socket(nullptr),
            listeningAddress(QString::fromUtf8(serverId))
        {
            mediatorConnector->setSystemCredentials(
                hpm::api::SystemCredentials(systemId, serverId, authKey));

            mediatorConnector->enable(true);
            mediatorConnection = mediatorConnector->systemConnection();
        }
    };

    bool registerOnMediatorSync()
    {
        QStringList sucessfulListening;
        QStringList failedListening;
        nx::utils::promise<void> registerPromise;
        {
            nx::utils::BarrierHandler barrier(
                [&registerPromise](){ registerPromise.set_value(); });

            for (auto& context: socketContexts)
            {
                context.socket->registerOnMediator(
                    [&, handler = barrier.fork(), contextPtr = &context](hpm::api::ResultCode code) mutable
                    {
                        if (code != hpm::api::ResultCode::ok)
                        {
                            failedListening << lm("%1(registeration: %2)")
                                .arg(context.listeningAddress)
                                .arg(QnLexical::serialized(code));

                            return handler();
                        }

                        if (!forwardedAddress)
                        {
                            sucessfulListening << context.listeningAddress;
                            return handler();
                        }

                        contextPtr->mediatorConnection->bind(
                            nx::hpm::api::BindRequest({*forwardedAddress}),
                            [&, handler = std::move(handler)](nx::hpm::api::ResultCode code)
                            {
                                if (code != hpm::api::ResultCode::ok)
                                {
                                    failedListening << lm("%1(bind: %2)")
                                        .arg(context.listeningAddress)
                                        .arg(QnLexical::serialized(code));
                                }
                                else
                                {
                                    sucessfulListening << context.listeningAddress;
                                }

                                return handler();
                            });
                    });
            }
        }

        registerPromise.get_future().wait();

        if (!failedListening.isEmpty())
        {
            limitStringList(&failedListening);
            std::cerr << "Warning: Addresses failed to listen: " <<
                failedListening.join(lit(", ")).toStdString() << std::endl;
        }

        if (!sucessfulListening.isEmpty())
        {
            limitStringList(&sucessfulListening);
            std::cout << "Listening on mediator, addresses: " <<
                sucessfulListening.join(lit(", ")).toStdString() << std::endl;
        }

        return !sucessfulListening.isEmpty();
    }

    std::vector<SocketContext> socketContexts;
    boost::optional<network::SocketAddress> forwardedAddress;
    boost::optional<hpm::api::ConnectionMethods> connectionMethonds;
};

int runInListenMode(const nx::utils::ArgumentParser& args)
{
    const auto beginTime = std::chrono::steady_clock::now();
    using namespace nx::network;

    auto transmissionMode = test::TestTransmissionMode::spam;
    if (args.get("ping"))
        transmissionMode = test::TestTransmissionMode::pong;
    std::cout << lm("Server mode: %1").args(transmissionMode).toStdString() << std::endl;

    auto multiServerSocket = new network::MultipleServerSocket();
    std::unique_ptr<AbstractStreamServerSocket> serverSocket(multiServerSocket);
    const auto guard = makeScopeGuard([&serverSocket]()
    {
        if (serverSocket)
            serverSocket->pleaseStopSync();
    });

    std::chrono::milliseconds rwTimeout = nx::network::test::TestConnection::kDefaultRwTimeout;
    {
        QString value;
        if (args.read("rw-timeout", &value))
            rwTimeout = nx::utils::parseTimerDuration(value, rwTimeout);

        stun::AbstractAsyncClient::Settings settings;
        settings.sendTimeout = rwTimeout;
        settings.recvTimeout = rwTimeout;
        hpm::api::MediatorConnector::setStunClientSettings(settings);
    }

    CloudServerSocketGenerator cloudServerSocketGenerator;
    test::RandomDataTcpServer server(
        test::TestTrafficLimitType::none, 0, transmissionMode, true);

    network::SocketAddress localAddress(network::HostAddress::localhost);
    if (const auto address = args.get("local-address"))
        localAddress = network::SocketAddress(*address);

    {
        std::unique_ptr<AbstractStreamServerSocket> localServer;
        if (args.get("udt"))
            localServer = std::make_unique<UdtStreamServerSocket>(AF_INET);
        else
            localServer = std::make_unique<TCPServerSocket>(AF_INET);

        if (!localServer->setReuseAddrFlag(true) ||
            !localServer->bind(localAddress))
        {
            std::cout << "Error: Unable to bind to " << localAddress.toString().toStdString() << std::endl;
            return 1;
        }

        localAddress = localServer->getLocalAddress();
        NX_CRITICAL(multiServerSocket->addSocket(std::move(localServer)));
        std::cout << "Listening on local " << (args.get("udt") ? "UDT" : "TCP")
            << " address " << localAddress.toString().toStdString() << std::endl;
    }

    if (const auto address = args.get("forward-address"))
    {
        // TODO: uncoment when CLOUD-730 is fixed
        // cloudServerSocketGenerator.connectionMethonds = hpm::api::ConnectionMethod::none;

        cloudServerSocketGenerator.forwardedAddress =
            address->isEmpty() ? localAddress : network::SocketAddress(*address);

        std::cout << "Mediator address forwarding: "
            << cloudServerSocketGenerator.forwardedAddress->toString().toStdString() << std::endl;
    }

    if (const auto credentials = args.get("cloud-credentials"))
    {
        const auto cloudCredentials = credentials->split(":");
        if (cloudCredentials.size() != 2)
        {
            std::cerr << "Error: Parameter cloud-credentials MUST have format "
                << "system_id:authentication_key" << std::endl;
            return 16;
        }

        String systemId = cloudCredentials[0].toUtf8();
        String authKey = cloudCredentials[1].toUtf8();
        std::vector<String> serverIds;
        {
            QString serverId = QnUuid::createUuid().toSimpleString().toUtf8();
            args.read("server-id", &serverId);
            serverIds.push_back(serverId.toUtf8());

            size_t serverCount = 1;
            args.read("server-count", &serverCount);
            for (size_t i = serverIds.size(); i < serverCount; i++)
                serverIds.push_back(makeServerName(serverId, i));
        }

        for (auto& id : serverIds)
        {
            auto socket = cloudServerSocketGenerator.make(systemId, authKey, id);
            if (!multiServerSocket->addSocket(std::move(socket)))
            {
                std::cout << "Error: could not add server " << id.data() << std::endl;
                return 15;
            }
        }

        if (!cloudServerSocketGenerator.registerOnMediatorSync())
        {
            std::cerr << "Error: All sockets failed to listen on mediator. " << std::endl;
            return 2;
        }
    }

    if (args.get("ssl"))
    {
        if (transmissionMode == test::TestTransmissionMode::spam)
        {
            std::cerr << "Error: spam mode does not support SSL, use --ping" << std::endl;
            return 7;
        }

        const auto certificate = network::ssl::Engine::makeCertificateAndKey(
            "cloud_connect_test_util", "US", "NX");

        if (certificate.isEmpty())
        {
            std::cerr << "Could not generate SSL certificate" << std::endl;
            return 4;
        }

        NX_CRITICAL(network::ssl::Engine::useCertificateAndPkey(certificate));
        serverSocket = std::make_unique<deprecated::SslServerSocket>(std::move(serverSocket), false);
    }

    server.setServerSocket(std::move(serverSocket));
    if (!server.start(rwTimeout))
    {
        auto osErrorText = SystemError::getLastOSErrorText().toStdString();
        std::cerr << "Error: Failed to start accepting connections. Reason: "
            << osErrorText << std::endl;
        return 5;
    }

    const auto passed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - beginTime);
    std::cout << "Sever has started in: " << passed.count() << " s." << std::endl;

    const int result = printStatsAndWaitForCompletion(
        &server,
        nx::utils::MoveOnlyFunc<bool()>());
    return result;
}

int printStatsAndWaitForCompletion(
    nx::network::test::ConnectionPool* const connectionPool,
    nx::utils::MoveOnlyFunc<bool()> interruptCondition)
{
    using nx::network::test::ConnectionTestStatistics;
    using namespace std::chrono;

    const std::chrono::seconds kUpdateStatisticsInterval(1);
    const std::chrono::minutes kStatisticsResetInterval(1);
    const ConnectionTestStatistics kZeroStatistics{0, 0, 0, 0};
    const ConnectionTestStatistics kInvalidStatistics{
        (uint64_t) -1, (uint64_t) -1, (size_t) -1, (size_t) -1};

    std::cout << "\nUsage statistics:" << std::endl;
    ConnectionTestStatistics prevStatistics = kInvalidStatistics;
    ConnectionTestStatistics baseStatisticsData = kZeroStatistics;
    boost::optional<steady_clock::time_point> sameStatisticsInterval;
    std::string prevStatToDisplayStr;
    DynamicStatistics dynamicStatistics;
    for (;;)
    {
        if (interruptCondition && interruptCondition())
            return 0;

        const auto data = connectionPool->statistics();
        auto statToDisplay = data - baseStatisticsData;

        if (statToDisplay != kZeroStatistics &&
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
                        << "Resetting statistics...\n" << std::endl;

                    //resetting statistics
                    baseStatisticsData = data;
                    baseStatisticsData.totalConnections -= baseStatisticsData.onlineConnections;
                    baseStatisticsData.onlineConnections = 0;

                    prevStatistics = kInvalidStatistics;
                    statToDisplay = kZeroStatistics;
                    sameStatisticsInterval.reset();
                }
            }
        }
        else
        {
            sameStatisticsInterval.reset();
        }

        dynamicStatistics.addValue(statToDisplay);

        prevStatistics = data;
        const auto statToDisplayStr =
            toString(statToDisplay).toStdString() + " " + dynamicStatistics.toStdString();

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

String makeServerName(const QString& prefix, size_t number)
{
    static const QString kFormat = QLatin1String("%1-%2");
    return kFormat.arg(prefix).arg((uint) number, 5, 10, QLatin1Char('0')).toUtf8();
}

void limitStringList(QStringList* list)
{
    if (list->size() <= kMaxAddressesToPrint)
        return;

    const auto size = list->size();
    list->erase(list->begin() + kMaxAddressesToPrint, list->end());
    *list << lm("... (%1 total)").arg(size);
}

} // namespace cctu
} // namespace nx
