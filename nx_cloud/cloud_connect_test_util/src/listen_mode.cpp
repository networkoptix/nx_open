#include "listen_mode.h"

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/ssl_socket.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/string.h>
#include <utils/common/ssl_gen_cert.h>

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

static const void limitStringList(QStringList* list)
{
    if (list->size() <= kMaxAddressesToPrint)
        return;

    const auto size = list->size();
    list->erase(list->begin() + kMaxAddressesToPrint, list->end());
    *list << lm("... (%1 total)").arg(size);
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

    std::vector<SocketContext> socketContexts;
};

int runInListenMode(const std::multimap<QString, QString>& args)
{
    using namespace nx::network;

    auto transmissionMode = test::TestTransmissionMode::spam;
    if (args.find("ping") != args.end())
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

    auto credentialsIter = args.find("cloud-credentials");
    auto localAddressIter = args.find("local-address");
    if (credentialsIter != args.end())
    {
        const auto cloudCredentials = credentialsIter->second.split(":");
        if (cloudCredentials.size() != 2)
        {
            std::cerr << "error. Parameter cloud-credentials MUST have format system_id:authentication_key" << std::endl;
            return 1;
        }

        std::vector<String> serverIds;
        {
            QString serverId = generateRandomName(7);
            readArg(args, "server-id", &serverId);
            serverIds.push_back(serverId.toUtf8());

            int serverCount = 1;
            readArg(args, "server-count", &serverCount);
            for (int i = serverIds.size(); i < serverCount; i++)
                serverIds.push_back((serverId + QString::number(i)).toUtf8());
        }

        serverSocket = cloudServerSocketGenerator.make(
            cloudCredentials[0].toUtf8(), cloudCredentials[1].toUtf8(), serverIds);

        QStringList sucessfulListening;
        QStringList failedListening;
        for (auto& context: cloudServerSocketGenerator.socketContexts)
        {
            if (context.socket->registerOnMediatorSync())
            {
                sucessfulListening << context.listeningAddress;
            }
            else
            {
                const auto error = SystemError::getLastOSErrorText();
                failedListening << lm("%1(%2)")
                    .arg(context.listeningAddress).arg(error);
            }
        }

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
        else
        {
            std::cerr << "error. All sockets failed to listen on mediator. "
                << std::endl;
            return 2;
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
        return 3;
    }

    if (args.find("ssl") != args.end())
    {
        const auto sslCert = tmpnam(nullptr);
        if (const auto ret = generateSslCertificate(sslCert, "cctu", "US", "cctu"))
            return ret;

        QFile file(QString::fromUtf8(sslCert));
        if(!file.open(QIODevice::ReadOnly))
        {
            std::cerr << "Could not generate SSL certificate" << std::endl;
            return 4;
        }

        nx::network::SslSocket::initSSLEngine(file.readAll());
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

} // namespace cctu
} // namespace nx
