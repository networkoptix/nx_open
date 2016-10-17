#include "client_mode.h"

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/cloud/tunnel/tcp/direct_endpoint_connector.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/test_support/socket_test_helper.h>

#include <utils/common/command_line_parser.h>
#include <nx/utils/string.h>

static const auto kDefaultTotalConnections = 100;
static const int kDefaultMaxConcurrentConnections = 1;
static const int kDefaultBytesToReceive = 1024 * 1024;

namespace nx {
namespace cctu {

void printConnectOptions(std::ostream* const outStream)
{
    *outStream <<
        "Connect mode:\n"
        "  --connect            Enable connect mode\n"
        "  --ping               Makes connections to verify server responses\n"
        "  --target={endpoint}  Regular or cloud address of server\n"
        "  --total-connections={"<< kDefaultTotalConnections <<"}\n"
        "                       Number of connections to try\n"
        "  --max-concurrent-connections={"<< kDefaultMaxConcurrentConnections <<"}\n"
        "  --bytes-to-receive={" << nx::utils::bytesToString(kDefaultBytesToReceive).toStdString() << "}\n"
        "                       Bytes to receive before closing connection\n"
        "  --bytes-to-send={N}  Bytes to send before closing connection\n"
        "  --forward-address    Use only forwarded address for connect"
        "  --udt                Force using udt socket. Disables cloud connect\n"
        "  --ssl                Use SSL on top of client sockets\n";
}

std::vector<SocketAddress> resolveTargets(
    SocketAddress targetAddress,
    const nx::utils::ArgumentParser& args)
{
    std::vector<SocketAddress> targets;
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
        targets.push_back(SocketAddress(serverId + systemSuffix, 0));
        for (std::size_t i = 1; i < serverCount; ++i)
        {
            targets.push_back(SocketAddress(
                serverId + QString::number(i).toUtf8() + systemSuffix, 0));
        }

        return targets;
    }

    // Or resolve it.
    std::promise<void> promise;
    const auto port = targetAddress.port;
    nx::network::SocketGlobals::addressResolver().resolveDomain(
        std::move(targetAddress.address),
        [port, &targets, &promise](std::vector<nx::network::cloud::TypedAddress> addresses)
        {
            for (auto& address: addresses)
                targets.push_back(SocketAddress(std::move(address.address), port));

            promise.set_value();
        });

    promise.get_future().wait();
    return targets;
}

int runInConnectMode(const nx::utils::ArgumentParser& args)
{
    QString target;
    if (!args.read("target", &target))
    {
        std::cerr << "Error: Required parameter \"target\" is missing" << std::endl;
        return 1;
    }

    nx::network::SocketGlobals::mediatorConnector().enable(true);

    int totalConnections = kDefaultTotalConnections;
    args.read("total-connections", &totalConnections);

    int maxConcurrentConnections = kDefaultMaxConcurrentConnections;
    args.read("max-concurrent-connections", &maxConcurrentConnections);

    nx::network::test::TestTrafficLimitType
        trafficLimitType = nx::network::test::TestTrafficLimitType::incoming;

    QString trafficLimit = nx::utils::bytesToString(kDefaultBytesToReceive);
    args.read("bytes-to-receive", &trafficLimit);

    if (args.read("bytes-to-send", &trafficLimit))
        trafficLimitType = nx::network::test::TestTrafficLimitType::outgoing; 

    auto transmissionMode = nx::network::test::TestTransmissionMode::spam;
    if (args.get("ping"))
        transmissionMode = nx::network::test::TestTransmissionMode::ping;

    if (args.get("forward-address"))
    {
        network::cloud::tcp::DirectEndpointConnector::setVerificationRequirement(false);
        network::cloud::ConnectorFactory::setEnabledCloudConnectMask(
            (int) network::cloud::CloudConnectType::forwardedTcpPort);
    }

    if (args.get("udt"))
        SocketFactory::enforceStreamSocketType(SocketFactory::SocketType::udt);

    if (args.get("ssl"))
    {
        if (transmissionMode == nx::network::test::TestTransmissionMode::spam)
        {
            std::cerr << "Error: Spam mode does not support SSL, use --ping" << std::endl;
            return 2;
        }

        SocketFactory::enforceSsl(true);
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
        .strs(transmissionMode, trafficLimitType, nx::utils::bytesToString(trafficLimitBytes),
            maxConcurrentConnections, totalConnections).toStdString() << std::endl;

    std::cout << lm("Target(s): %1").arg(targetStrings.join(lit(", "))).toStdString() << std::endl;
    nx::network::test::ConnectionsGenerator connectionsGenerator(
        std::move(targetList),
        maxConcurrentConnections,
        trafficLimitType,
        trafficLimitBytes,
        totalConnections,
        transmissionMode);

    std::promise<void> finishedPromise;
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

    QStringList returnCodes;
    for (const auto& code : connectionsGenerator.returnCodes())
        returnCodes << lm("%2 [%1 time(s)]").arg(code.second)
            .arg(SystemError::toString(code.first));

    std::cout << "\n\nConnect summary: \n"
        "  total time: " << testDuration.count() << "s\n"
        "  total connections: " <<
            connectionsGenerator.totalConnectionsEstablished() << "\n"
        "  total bytes sent: " <<
            nx::utils::bytesToString(connectionsGenerator.totalBytesSent()).toStdString() << "\n"
        "  total bytes received: " <<
            nx::utils::bytesToString(connectionsGenerator.totalBytesReceived()).toStdString() << "\n"
        "  total incomplete tasks: " <<
            connectionsGenerator.totalIncompleteTasks() << "\n"
        "  return codes: " << returnCodes.join(QLatin1Literal("; ")).toStdString() << "\n" <<
            std::endl;

    return 0;
}

void printHttpClientOptions(std::ostream* const outStream)
{
    *outStream <<
        "Http client mode:\n"
        "  --http-client        Enable Http client mode\n"
        "  --url={http url}     Url to trigger\n";
}

int runInHttpClientMode(const nx::utils::ArgumentParser& args)
{
    QString urlStr;
    if (!args.read("url", &urlStr))
    {
        std::cerr << "Error: Required parameter \"url\" is missing" << std::endl;
        return 1;
    }

    nx::network::SocketGlobals::mediatorConnector().enable(true);

    NX_LOG(lm("Issuing request to %1").arg(urlStr), cl_logALWAYS);

    nx_http::HttpClient client;
    client.setSendTimeoutMs(15000);
    if (!client.doGet(urlStr) || !client.response())
    {
        std::cerr<<"No response has been received"<<std::endl;
        return 1;
    }

    std::cout<<"Received response:\n\n"
        << client.response()->toString().toStdString()
        <<"\n";

    if (nx_http::getHeaderValue(client.response()->headers, "Content-Type") == "application/json")
    {
        while (!client.eof())
        {
            const auto buf = client.fetchMessageBodyBuffer();
            std::cout<<buf.constData();
        }
    }

    std::cout << std::endl;
    NX_LOG(lm("Rompleted request to %1").arg(urlStr), cl_logALWAYS);
    return 0;
}

} // namespace cctu
} // namespace nx
