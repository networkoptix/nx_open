/**********************************************************
* Mar 3, 2016
* a.kolesnikov
***********************************************************/

#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/socket_test_helper.h>

#include <utils/common/command_line_parser.h>

#include "listen_mode.h"


void printHelp(int argc, char* argv[]);
int runInListenMode(const std::multimap<QString, QString>& args);
int runInConnectMode(const std::multimap<QString, QString>& args);
int runInHttpClientMode(const std::multimap<QString, QString>& args);

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
        return runInListenMode(args);

    if (args.find("connect") != args.end())
        return runInConnectMode(args);

    if (args.find("http-client") != args.end())
        return runInHttpClientMode(args);

    std::cerr<<"error. Unknown mode"<<std::endl;
    printHelp(argc, argv);

    return 0;
}

const auto kDefaultTotalConnections = 100;
const int kDefaultMaxConcurrentConnections = 1;
const int kDefaultBytesToReceive = 1024 * 1024;

static int stringToBytes(const QString& value)
{
    bool isOk;
    int ret;
    const auto subint = [&](int multi)
    {
        ret = QStringRef(&value, 0, value.size() - 1).toInt(&isOk) * multi;
    };

    if (value.endsWith('k', Qt::CaseInsensitive)) subint(1024);
    else
    if (value.endsWith('m', Qt::CaseInsensitive)) subint(1024 * 1024);
    else
    if (value.endsWith('g', Qt::CaseInsensitive)) subint(1024 * 1024 * 1024);
    else
        ret = value.toInt(&isOk);

    if (!isOk)
    {
        ret = kDefaultBytesToReceive;
        std::cerr << "Can not convert '" << value.toStdString() << "' to bytes, "
            "use " << ret << " instead" << std::endl;
    }

    return ret;
}

QString bytesToString(int value)
{
    static const std::vector<const char*> kSuffixes = { "", "k", "m", "g" };

    float dValue = static_cast<float>(value);
    size_t index = 0;
    while (index < kSuffixes.size() && dValue >= 1024.0)
    {
        index++;
        dValue /= 1024;
    }

    return lm("%1%2").arg(dValue).arg(kSuffixes[index]);
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
    else
    {
        SocketFactory::setCreateStreamSocketFunc(
            [](bool /*ssl*/, SocketFactory::NatTraversalType /*ntt*/)
            {
                return std::make_unique<nx::network::cloud::CloudStreamSocket>();
            });
    }

    nx::network::test::ConnectionsGenerator connectionsGenerator(
        SocketAddress(target),
        maxConcurrentConnections,
        trafficLimitType,
        stringToBytes(trafficLimit),
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

    std::cout << "\n\nconnect summary: "
        "  total time: " << testDuration.count() << "s\n"
        "  total connections: " <<
            connectionsGenerator.totalConnectionsEstablished() << "\n"
        "  total bytes sent: " <<
            bytesToString(connectionsGenerator.totalBytesSent()).toStdString() << "\n"
        "  total bytes received: " <<
            bytesToString(connectionsGenerator.totalBytesReceived()).toStdString() << "\n"
        "  total incomplete tasks: " <<
            connectionsGenerator.totalIncompleteTasks() << "\n\n"
        "  report: " << connectionsGenerator.returnCodes().toStdString() << "\n" <<
            std::endl;

    return 0;
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

void printHelp(int /*argc*/, char* /*argv*/[])
{
    std::cout << "\n";
    printListenOptions(&std::cout);
    std::cout <<
        "\n"
        "Connect mode:\n"
        "  --connect                        Enable connect mode\n"
        "  --echo                           Makes connections to verify server responses\n"
        "  --target={endpoint}              Regular or cloud address of server\n"
        "  --total-connections={"<< kDefaultTotalConnections <<"}\n"
        "                                   Number of connections to try\n"
        "  --max-concurrent-connections={"<< kDefaultMaxConcurrentConnections <<"}\n"
        "  --bytes-to-receive={"<< bytesToString(kDefaultBytesToReceive).toStdString() <<"}\n"
        "                                   Bytes to receive before closing connection\n"
        "  --bytes-to-send=                 Bytes to send before closing connection\n"
        "  --udt                            Use udt instead of tcp\n"
        "\n"
        "Http client mode:\n"
        "  --http-client                    Enable Http client mode\n"
        "  --url={http url}                 Url to trigger\n"
        "\n"
        "Common options:\n"
        "  --enforce-mediator={endpoint}    Enforces custom mediator address\n"
        "  --log-level={level}              Log level"
        "\n"
        << std::endl;
}


//--http-client --url=http://admin:admin@server1.ffc8e5a2-a173-4b3d-8627-6ab73d6b234d/api/gettime
//AK server:
//--http-client --url=http://admin:admin@47bf37a0-72a6-2890-b967-5da9c390d28a.ec0fc8ba-9dd1-417f-aa88-f36400953ade/api/gettime
//LA server:
//--http-client --url=http://admin:admin@1af3ebeb-c327-3665-40f1-fa4dba0df78f.ec0fc8ba-9dd1-417f-aa88-f36400953ade/api/gettime
//--connect --target=server1.ec0fc8ba-9dd1-417f-aa88-f36400953ade --max-concurrent-connections=10
//--listen --server-id=server1 --cloud-credentials=ec0fc8ba-9dd1-417f-aa88-f36400953ade:1cc88e0d-1c08-4051-ae01-dfe3f5b5b90a

/** AK old PC mediator test:

--enforce-mediator=10.0.2.41:3345 --listen --echo --cloud-credentials=93e0467f-3145-41a8-8ebc-7f3c95e2ccf0:32cfaaf7-19fe-4bb2-a06d-4b6bac489757 --server-id=xxx
--enforce-mediator=10.0.2.41:3345 --connect --echo --target=xxx.93e0467f-3145-41a8-8ebc-7f3c95e2ccf0 --bytes-to-recieve=1m --total-connections=5 --max-concurrent-connections=5


--listen --local-address=0.0.0.0:5724 --udt
--connect --target=192.168.0.1:5724 --udt
--connect --target=192.168.0.1:5724 --udt --total-connections=1 --bytes-to-receive=100000000
*/
