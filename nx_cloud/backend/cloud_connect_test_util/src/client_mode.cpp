#include "client_mode.h"

#include <fstream>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/cross_nat_connector.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/cloud/tunnel/tcp/direct_endpoint_connector.h>
#include <nx/network/http/http_client.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/string.h>
#include <nx/utils/timer_manager.h>

static const auto kDefaultTotalConnections = 100;
static const int kDefaultMaxConcurrentConnections = 1;
static const int kDefaultBytesToReceive = 1024 * 1024;

namespace nx {
namespace cctu {

void printConnectOptions(std::ostream* const outStream)
{
    *outStream <<
        "Connect mode:\n"
        "  --connect                        Enable connect mode\n"
        "  --ping                           Makes connections to verify server responses\n"
        "  --target={endpoint}              Regular or cloud address of server\n"
        "  --total-connections={"<< kDefaultTotalConnections <<"}\n"
        "                                   Number of connections to try\n"
        "  --max-concurrent-connections={"<< kDefaultMaxConcurrentConnections <<"}\n"
        "  --bytes-to-receive={N}\n"
        "                                   Bytes to receive before closing connection. No limit by default\n"
        "  --bytes-to-send={N}              Bytes to send before closing connection. No limit by default\n"
        "  --forward-address                Use only forwarded address for connect\n"
        "  --udt                            Force using udt socket. Disables cloud connect\n"
        "  --ssl                            Use SSL on top of client sockets\n";
}

static bool resolveDomainName(
    const network::SocketAddress& targetAddress,
    std::vector<network::SocketAddress>* instanceEndpoints);

static std::vector<network::SocketAddress> resolveTargets(
    network::SocketAddress targetAddress,
    const nx::utils::ArgumentParser& args)
{
    std::vector<network::SocketAddress> targets;
    if (targetAddress.address.toString().contains('.'))
    {
        targets.push_back(std::move(targetAddress));
        return targets;
    }

    // It's likely a system id, add server ids if available.
    QString serverId;
    size_t serverCount;
    if (args.read("server-id", &serverId) && args.read("server-count", &serverCount))
    {
        const auto systemSuffix = '.' + targetAddress.address.toString().toUtf8();
        targets.push_back(network::SocketAddress(serverId + systemSuffix, 0));
        for (size_t i = 1; i < serverCount; ++i)
            targets.push_back(network::SocketAddress(makeServerName(serverId, i) + systemSuffix));

        return targets;
    }

    resolveDomainName(targetAddress, &targets);
    return targets;
}

static bool resolveDomainName(
    const network::SocketAddress& targetAddress,
    std::vector<network::SocketAddress>* instanceEndpoints)
{
    auto mediatorConnection = nx::network::SocketGlobals::cloud().mediatorConnector().clientConnection();
    auto mediatorConnectionGuard =
        nx::utils::makeScopeGuard([&mediatorConnection]() { mediatorConnection->pleaseStopSync(); });

    nx::utils::promise<
        std::tuple<nx::hpm::api::ResultCode, nx::hpm::api::ResolveDomainResponse>
    > resolveDomainDone;
    mediatorConnection->resolveDomain(
        nx::hpm::api::ResolveDomainRequest(targetAddress.address.toString().toUtf8()),
        [&resolveDomainDone](
            nx::hpm::api::ResultCode resultCode,
            nx::hpm::api::ResolveDomainResponse response)
        {
            resolveDomainDone.set_value(std::make_tuple(resultCode, std::move(response)));
        });

    const auto result = resolveDomainDone.get_future().get();
    if (std::get<0>(result) != nx::hpm::api::ResultCode::ok)
        return false;

    for (const auto& hostName: std::get<1>(result).hostNames)
    {
        const auto entries = nx::network::SocketGlobals::addressResolver().resolveSync(
            hostName.toStdString(),
            nx::network::NatTraversalSupport::enabled,
            AF_INET);
        for (const auto& entry: entries)
            instanceEndpoints->push_back(network::SocketAddress(entry.host, targetAddress.port));
    }

    return true;
}

int runInConnectMode(const nx::utils::ArgumentParser& args)
{
    auto& mediatorResultCounter = network::cloud::CrossNatConnector::mediatorResponseCounter();
    mediatorResultCounter.enable();

    QString target;
    if (!args.read("target", &target))
    {
        std::cerr << "Error: Required parameter \"target\" is missing" << std::endl;
        return 1;
    }

    nx::network::SocketGlobals::cloud().mediatorConnector().enable(true);
    nx::network::SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId(
        "cc-tu-connect", QnUuid::createUuid());

    int totalConnections = kDefaultTotalConnections;
    args.read("total-connections", &totalConnections);

    int maxConcurrentConnections = kDefaultMaxConcurrentConnections;
    args.read("max-concurrent-connections", &maxConcurrentConnections);

    nx::network::test::TestTrafficLimitType
        trafficLimitType = nx::network::test::TestTrafficLimitType::none;

    QString trafficLimit = nx::utils::bytesToString(kDefaultBytesToReceive);
    if (args.read("bytes-to-receive", &trafficLimit))
        trafficLimitType = nx::network::test::TestTrafficLimitType::incoming;

    if (args.read("bytes-to-send", &trafficLimit))
        trafficLimitType = nx::network::test::TestTrafficLimitType::outgoing;

    auto transmissionMode = nx::network::test::TestTransmissionMode::spam;
    if (args.get("ping"))
        transmissionMode = nx::network::test::TestTransmissionMode::ping;
    if (args.get("receive-only"))
        transmissionMode = nx::network::test::TestTransmissionMode::receiveOnly;

    if (args.get("forward-address"))
    {
        network::cloud::tcp::DirectEndpointConnector::setVerificationRequirement(false);
        network::cloud::ConnectorFactory::setEnabledCloudConnectMask(
            (int) network::cloud::ConnectType::forwardedTcpPort);
    }

    if (args.get("udt"))
        nx::network::SocketFactory::enforceStreamSocketType(nx::network::SocketFactory::SocketType::udt);

    if (args.get("ssl"))
    {
        if (transmissionMode == nx::network::test::TestTransmissionMode::spam)
        {
            std::cerr << "Error: Spam mode does not support SSL, use --ping" << std::endl;
            return 2;
        }

        nx::network::SocketFactory::enforceSsl(true);
    }

    std::chrono::milliseconds rwTimeout = nx::network::test::TestConnection::kDefaultRwTimeout;
    {
        QString value;
        if (args.read("rw-timeout", &value))
            rwTimeout = nx::utils::parseTimerDuration(value, rwTimeout);
    }

    const auto targetList = resolveTargets(target, args);
    if (targetList.empty())
    {
        std::cerr << "Error: There are no targets to connect to!" << std::endl;
        return 1;
    }

    uint64_t trafficLimitBytes = nx::utils::stringToBytes(trafficLimit);
    trafficLimitBytes = trafficLimitBytes ? trafficLimitBytes : kDefaultBytesToReceive;

    QStringList targetStrings;
    for (const auto t: targetList)
        targetStrings << t.toString();

    limitStringList(&targetStrings);
    std::cout << lm("Client mode: %1, limit: %2(%3b), max concurent connections: %4, total: %5")
        .args(transmissionMode, trafficLimitType, nx::utils::bytesToString(trafficLimitBytes),
            maxConcurrentConnections, totalConnections).toStdString() << std::endl;

    std::cout << lm("Target(s): %1").arg(targetStrings.join(lit(", "))).toStdString() << std::endl;
    nx::network::test::ConnectionsGenerator connectionsGenerator(
        std::move(targetList),
        maxConcurrentConnections,
        trafficLimitType,
        trafficLimitBytes,
        totalConnections,
        transmissionMode);

    nx::utils::promise<void> finishedPromise;
    connectionsGenerator.setOnFinishedHandler(
        [&finishedPromise]{ finishedPromise.set_value(); });

    const auto startTime = std::chrono::steady_clock::now();
    connectionsGenerator.start(rwTimeout);

    auto finishedFuture = finishedPromise.get_future();
    printStatsAndWaitForCompletion(
        &connectionsGenerator,
        [&finishedFuture]() -> bool
        {
            return finishedFuture.wait_for(std::chrono::seconds::zero())
                == std::future_status::ready;
        });

    const auto testDuration =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime);

    std::cout << "\n\nConnect summary:\n"
        "  total time: " << testDuration.count() << " s\n"
        "  total connections: " <<
            connectionsGenerator.totalConnectionsEstablished() << "\n"
        "  total bytes sent: " <<
            nx::utils::bytesToString(connectionsGenerator.totalBytesSent()).toStdString() << "\n"
        "  total bytes received: " <<
            nx::utils::bytesToString(connectionsGenerator.totalBytesReceived()).toStdString() << "\n"
        "  total incomplete tasks: " <<
            connectionsGenerator.totalIncompleteTasks() << "\n"
        "  mediator results: " <<
            mediatorResultCounter.stringStatus().toStdString() << "\n" <<
        "  connect results: " <<
            connectionsGenerator.results().stringStatus().toStdString() << "\n" <<
        std::endl;

    return 0;
}

void printHttpClientOptions(std::ostream* const outStream)
{
    *outStream <<
        "Http client mode:\n"
        "  --http-client                    Enable Http client mode\n"
        "  --url={http url}                 Url to trigger\n"
        "\n"
        "  -o file\n"
        "  --output-document=file           Save message received to the file specified\n";
}

int runInHttpClientMode(const nx::utils::ArgumentParser& args)
{
    QString urlStr;
    if (!args.read("url", &urlStr))
    {
        std::cerr << "Error: Required parameter \"url\" is missing" << std::endl;
        return 1;
    }

    QString messageBodyFilePath;
    args.read("o", &messageBodyFilePath);
    args.read("output-document", &messageBodyFilePath);

    nx::network::SocketGlobals::cloud().mediatorConnector().enable(true);
    nx::network::SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId(
        "cc-tu-http", QnUuid::createUuid());

    NX_LOG(lm("Issuing request to %1").arg(urlStr), cl_logALWAYS);

    nx::network::http::HttpClient client;
    client.setSendTimeout(std::chrono::seconds(15));
    if (!client.doGet(urlStr) || !client.response())
    {
        std::cerr<<"No response has been received"<<std::endl;
        return 1;
    }

    std::cout<<"Received response:\n\n"
        << client.response()->toString().toStdString()
        <<"\n";

    std::ostream* outputStream = nullptr;
    std::unique_ptr<std::ofstream> outputFile;

    if (!messageBodyFilePath.isEmpty())
    {
        outputFile = std::make_unique<std::ofstream>(
            messageBodyFilePath.toStdString().c_str(),
            std::ios_base::binary | std::ios_base::out);
        if (!outputFile->is_open())
        {
            std::cerr << "Failed to open output file " <<
                messageBodyFilePath.toStdString() << std::endl;
            return 1;
        }

        outputStream = outputFile.get();
    }
    else
    {
        const auto contentType =
            nx::network::http::getHeaderValue(client.response()->headers, "Content-Type");
        if (contentType == "application/json" ||
            contentType == "text/xml")
        {
            outputStream = &std::cout;
        }
    }

    if (outputStream)
    {
        while (!client.eof())
        {
            const auto buf = client.fetchMessageBodyBuffer();
            (*outputStream) << buf.toStdString();
        }
        outputStream->flush();
    }

    std::cout << std::endl;
    NX_LOG(lm("Completed request to %1").arg(urlStr), cl_logALWAYS);
    return 0;
}

} // namespace cctu
} // namespace nx
