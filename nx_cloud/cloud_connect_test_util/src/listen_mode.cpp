#include "listen_mode.h"

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/ssl_socket.h>

#include <utils/common/command_line_parser.h>
#include <nx/utils/string.h>
#include <nx/fusion/serialization/lexical.h>

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
    "  --local-address={ip:port}    Local address to listen\n"
    "  --udt                        Use udt instead of tcp. Only if listening local address\n"
    "  --ssl                        Uses SSL on top of server socket type\n";
}

class CloudServerSocketGenerator
{
public:
    std::unique_ptr<AbstractStreamServerSocket> make(
        String systemId, String authKey, std::vector<String> serverIds)
    {
        auto multipleServerSocket = std::make_unique<network::MultipleServerSocket>();
        for (auto& id : serverIds)
        {
            auto socket = make(systemId, authKey, id);
            if (!socket)
                return nullptr;

            if (!multipleServerSocket->addSocket(std::move(socket)))
                return nullptr;
        }

        return std::move(multipleServerSocket);
    }

    std::unique_ptr<AbstractStreamServerSocket> make(
        String systemId, String authKey, String serverId)
    {
        socketContexts.push_back(SocketContext(
            std::move(systemId), std::move(authKey), std::move(serverId)));

        auto& currentContext = socketContexts.back();
        auto socket = std::make_unique<network::cloud::CloudServerSocket>(
            currentContext.mediatorConnector->systemConnection());

        currentContext.socket = socket.get();
        return std::move(socket);
    }

    struct SocketContext
    {
        std::unique_ptr<hpm::api::MediatorConnector> mediatorConnector;
        network::cloud::CloudServerSocket* socket;
        QString listeningAddress;
        SystemError::ErrorCode listeningStatus;

        SocketContext(const SocketContext&) = delete;
        SocketContext(SocketContext&&) = default;
        SocketContext& operator=(const SocketContext&) = delete;
        SocketContext& operator=(SocketContext&&) = default;

        SocketContext(String systemId, String authKey, String serverId)
        :
            mediatorConnector(new hpm::api::MediatorConnector),
            socket(nullptr),
            listeningAddress(QString::fromUtf8(serverId)),
            listeningStatus(SystemError::noError)
        {
            mediatorConnector->mockupAddress(network::SocketGlobals::mediatorConnector());
            mediatorConnector->setSystemCredentials(
                hpm::api::SystemCredentials(systemId, serverId, authKey));
            mediatorConnector->enable(true);
        }
    };

    ~CloudServerSocketGenerator()
    {
        for (auto& context: socketContexts)
            context.socket->pleaseStopSync();
    }

    bool registerOnMediatorSync()
    {
        QStringList sucessfulListening;
        QStringList failedListening;
        nx::utils::promise<void> registerPromise;
        {
            nx::BarrierHandler barrier(
                [&registerPromise](){ registerPromise.set_value(); });

            for (auto& context: socketContexts)
            {
                context.socket->registerOnMediator(
                    [&, handler = barrier.fork()](hpm::api::ResultCode code)
                    {
                        if (code == hpm::api::ResultCode::ok)
                        {
                            sucessfulListening << context.listeningAddress;
                        }
                        else
                        {
                            failedListening << lm("%1(%2)")
                                .arg(context.listeningAddress)
                                .arg(QnLexical::serialized(code));
                        }

                        handler();
                    });
            }
        }

        registerPromise.get_future().wait();

        if (!failedListening.isEmpty())
        {
            limitStringList(&failedListening);
            std::cerr << "warning. Addresses failed to listen: " <<
                failedListening.join(lit(", ")).toStdString() << std::endl;
        }

        if (!sucessfulListening.isEmpty())
        {
            limitStringList(&sucessfulListening);
            std::cout << "listening on mediator. Addresses: " <<
                sucessfulListening.join(lit(", ")).toStdString() << std::endl;
        }

        return !sucessfulListening.isEmpty();
    }

    std::vector<SocketContext> socketContexts;
};

int runInListenMode(const nx::utils::ArgumentParser& args)
{
    using namespace nx::network;

    auto transmissionMode = test::TestTransmissionMode::spam;
    if (args.get("ping"))
        transmissionMode = test::TestTransmissionMode::pong;

    std::unique_ptr<AbstractStreamServerSocket> serverSocket;
    const auto guard = makeScopedGuard([&serverSocket]()
    {
        if (serverSocket)
            serverSocket->pleaseStopSync();
    });

    CloudServerSocketGenerator cloudServerSocketGenerator;
    test::RandomDataTcpServer server(
        test::TestTrafficLimitType::none, 0, transmissionMode);

    if (const auto credentials = args.get("cloud-credentials"))
    {
        const auto cloudCredentials = credentials->split(":");
        if (cloudCredentials.size() != 2)
        {
            std::cerr << "error. Parameter cloud-credentials MUST have format system_id:authentication_key" << std::endl;
            return 1;
        }

        std::vector<String> serverIds;
        {
            QString serverId = nx::utils::generateRandomName(7);
            args.read("server-id", &serverId);
            serverIds.push_back(serverId.toUtf8());

            int serverCount = 1;
            args.read("server-count", &serverCount);
            for (int i = serverIds.size(); i < serverCount; i++)
                serverIds.push_back((serverId + QString::number(i)).toUtf8());
        }

        serverSocket = cloudServerSocketGenerator.make(
            cloudCredentials[0].toUtf8(), cloudCredentials[1].toUtf8(), serverIds);

        if (!cloudServerSocketGenerator.registerOnMediatorSync())
        {
            std::cerr << "error. All sockets failed to listen on mediator. "
                << std::endl;
            return 2;
        }
    }
    else if (const auto localAddress = args.get("local-address"))
    {
        if (args.get("udt"))
            serverSocket = std::make_unique<UdtStreamServerSocket>();
        else
            serverSocket = std::make_unique<TCPServerSocket>();

        server.setLocalAddress(SocketAddress(*localAddress));
        std::cout << "listening on local " << (args.get("udt") ? "udt" : "tcp")
            << " address " << localAddress->toStdString() << std::endl;
    }
    else
    {
        std::cerr << "error. You have to specifiy --cloud-credentials or --local-address " << std::endl;
        return 3;
    }

    if (args.get("ssl"))
    {
        if (transmissionMode == test::TestTransmissionMode::spam)
        {
            std::cerr << "error. Spam mode does not support SSL, use --ping" << std::endl;
            return 7;
        }

        const auto certificate = network::SslEngine::makeCertificateAndKey(
            "cloud_connect_test_util", "US", "NX");

        if (certificate.isEmpty())
        {
            std::cerr << "Could not generate SSL certificate" << std::endl;
            return 4;
        }

        NX_CRITICAL(network::SslEngine::useCertificateAndPkey(certificate));
        serverSocket.reset(new SslServerSocket(serverSocket.release(), false));
    }

    server.setServerSocket(std::move(serverSocket));
    if (!server.start())
    {
        auto osErrorText = SystemError::getLastOSErrorText().toStdString();
        std::cerr << "error. Failed to start accepting connections. Reason: "
            << osErrorText << std::endl;
        return 5;
    }

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

    constexpr const auto kUpdateStatisticsInterval = std::chrono::seconds(1);
    constexpr const auto kStatisticsResetInterval = std::chrono::seconds(10);
    constexpr const auto zeroStatistics = ConnectionTestStatistics{ 0, 0, 0, 0 };
    constexpr const auto invalidStatistics =
        ConnectionTestStatistics{ (uint64_t)-1, (uint64_t)-1, (size_t)-1, (size_t)-1 };

    std::cout << "\nUsage statistics:" << std::endl;
    ConnectionTestStatistics prevStatistics = invalidStatistics;
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
                        << "Resetting statistics...\n" << std::endl;

                    //resetting statistics
                    baseStatisticsData = data;
                    prevStatistics = invalidStatistics;
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
