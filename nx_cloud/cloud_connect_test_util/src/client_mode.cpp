#include "client_mode.h"

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/test_support/socket_test_helper.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/string.h>

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
        "  --echo               Makes connections to verify server responses\n"
        "  --target={endpoint}  Regular or cloud address of server\n"
        "  --total-connections={"<< kDefaultTotalConnections <<"}\n"
        "                       Number of connections to try\n"
        "  --max-concurrent-connections={"<< kDefaultMaxConcurrentConnections <<"}\n"
        "  --bytes-to-receive={"<< bytesToString(kDefaultBytesToReceive).toStdString() <<"}\n"
        "                        Bytes to receive before closing connection\n"
        "  --bytes-to-send={N}   Bytes to send before closing connection\n"
        "  --udt                 Use udt instead of tcp\n"
        "  --ssl                 Use SSL on top of client sockets\n";
}

int runInConnectMode(const std::multimap<QString, QString>& args)
{
    QString target;
    if (!readArg(args, "target", &target))
    {
        std::cerr << "error. Required parameter \"target\" is missing" << std::endl;
        return 1;
    }

    nx::network::SocketGlobals::mediatorConnector().enable(true);

    int totalConnections = kDefaultTotalConnections;
    readArg(args, "total-connections", &totalConnections);

    int maxConcurrentConnections = kDefaultMaxConcurrentConnections;
    readArg(args, "max-concurrent-connections", &maxConcurrentConnections);

    nx::network::test::TestTrafficLimitType
        trafficLimitType = nx::network::test::TestTrafficLimitType::incoming;

    QString trafficLimit = bytesToString(kDefaultBytesToReceive);
    readArg(args, "bytes-to-receive", &trafficLimit);

    if (readArg(args, "bytes-to-send", &trafficLimit))
        trafficLimitType = nx::network::test::TestTrafficLimitType::outgoing;

    auto transmissionMode = nx::network::test::TestTransmissionMode::spam;
    if (args.find("echo") != args.end())
        transmissionMode = nx::network::test::TestTransmissionMode::echoTest;

    if (args.find("udt") != args.end())
    {
        SocketFactory::enforceStreamSocketType(SocketFactory::SocketType::udt);
    }

    if (args.find("ssl") != args.end())
        SocketFactory::enforceSsl(true);

    const uint64_t trafficLimitBytes = stringToBytes(trafficLimit);
    nx::network::test::ConnectionsGenerator connectionsGenerator(
        SocketAddress(target),
        maxConcurrentConnections,
        trafficLimitType,
        trafficLimitBytes != 0 ? trafficLimitBytes : kDefaultBytesToReceive,
        totalConnections,
        transmissionMode);

    std::promise<void> finishedPromise;
    connectionsGenerator.setOnFinishedHandler(
        [&finishedPromise]{ finishedPromise.set_value(); });

    const auto startTime = std::chrono::steady_clock::now();
    connectionsGenerator.start();

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

    std::cout << "\n\nconnect summary: \n"
        "  total time: " << testDuration.count() << "s\n"
        "  total connections: " <<
            connectionsGenerator.totalConnectionsEstablished() << "\n"
        "  total bytes sent: " <<
            bytesToString(connectionsGenerator.totalBytesSent()).toStdString() << "\n"
        "  total bytes received: " <<
            bytesToString(connectionsGenerator.totalBytesReceived()).toStdString() << "\n"
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

int runInHttpClientMode(const std::multimap<QString, QString>& args)
{
    QString urlStr;
    if (!readArg(args, "url", &urlStr))
    {
        std::cerr << "error. Required parameter \"url\" is missing" << std::endl;
        return 1;
    }

    nx::network::SocketGlobals::mediatorConnector().enable(true);

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
    return 0;
}

} // namespace cctu
} // namespace nx
