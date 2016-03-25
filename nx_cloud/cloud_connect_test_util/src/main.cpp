/**********************************************************
* Mar 3, 2016
* a.kolesnikov
***********************************************************/

#include <iostream>

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/socket_test_helper.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/string.h>


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

int runInListenMode(const std::multimap<QString, QString>& args)
{
    using namespace nx::network;

    auto credentialsIter = args.find("cloud-credentials");
    if (credentialsIter == args.end())
    {
        std::cerr<<"error. Required parameter cloud-credentials is missing"<<std::endl;
        return 1;
    }
    const auto credentials = credentialsIter->second.split(":");
    if (credentials.size() != 2)
    {
        std::cerr << "error. Required parameter cloud-credentials MUST have format system_id:authentication_key" << std::endl;
        return 1;
    }
    QString serverId = generateRandomName(7);
    readArg(args, "server-id", &serverId);

    auto transmissionMode = nx::network::test::TestTransmissionMode::spam;
    if (args.find("echo") != args.end())
        transmissionMode = nx::network::test::TestTransmissionMode::echo;

    SocketGlobals::mediatorConnector().setSystemCredentials(
        nx::hpm::api::SystemCredentials(
            credentials[0].toUtf8(),
            serverId.toUtf8(),
            credentials[1].toUtf8()));
    SocketGlobals::mediatorConnector().enable(true);

    auto serverSock = std::make_unique<cloud::CloudServerSocket>(
        SocketGlobals::mediatorConnector().systemConnection());
    if (!serverSock->registerOnMediatorSync())
    {
        std::cerr << "error. Failed to listen on mediator. Reason: " <<
            SystemError::getLastOSErrorText().toStdString() << std::endl;
        return 1;
    }
    std::cout << "listening on mediator. Address "
        << serverId.toStdString()<<"."<< credentials[0].toStdString()
        << std::endl;

    //TODO #ak RandomDataTcpServer does not fit well at the moment: 
        //should fit it to this tool's needs

    test::RandomDataTcpServer server(
        test::TestTrafficLimitType::none, 0, transmissionMode);
    server.setServerSocket(std::move(serverSock));
    if (!server.start())
    {
        std::cerr << "error. Failed to start accepting connections. Reason: " <<
            SystemError::getLastOSErrorText().toStdString() << std::endl;
        return 1;
    }

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

    server.pleaseStopSync();
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

    SocketFactory::setCreateStreamSocketFunc(
        [](bool ssl, SocketFactory::NatTraversalType ntt)
        {
            static_cast<void>(ssl);
            static_cast<void>(ntt);
            return std::make_unique<nx::network::cloud::CloudStreamSocket>();
        });

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
    finishedPromise.get_future().wait();
    const auto testDuration =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime);

    std::cout << "connect summary: "
        "Total time: " << testDuration.count() << "s. "
        "Total connections: " <<
            connectionsGenerator.totalConnectionsEstablished() << ". "
        "Total bytes sent: " <<
            bytesToString(connectionsGenerator.totalBytesSent()).toStdString() << ". "
        "Total bytes received: " <<
            bytesToString(connectionsGenerator.totalBytesReceived()).toStdString() << ". "
        "Total incomplete tasks: " <<
            connectionsGenerator.totalIncompleteTasks() << ". \n"
        "report: " << connectionsGenerator.returnCodes().toStdString() << ". " <<
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
    std::cout << 
        "\n"
        "Listen mode:\n"
        "  --listen                         Enable listen mode\n"
        "  --echo                           Makes server to mirror data instead of spaming\n"
        "  --cloud-credentials={system_id}:{authentication_key}\n"
        "                                   Specify credentials to use to connect to mediator\n"
        "  --server-id={server_id}          Id used when registering on mediator\n"
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
//--http-client --url=http://admin:admin@47bf37a0-72a6-2890-b967-5da9c390d28a.ffc8e5a2-a173-4b3d-8627-6ab73d6b234d/api/gettime
//LA server:
//--http-client --url=http://admin:admin@1af3ebeb-c327-3665-40f1-fa4dba0df78f.ffc8e5a2-a173-4b3d-8627-6ab73d6b234d/api/gettime
//--connect --target=server1.ffc8e5a2-a173-4b3d-8627-6ab73d6b234d --max-concurrent-connections=1
//--listen --server-id=server1 --cloud-credentials=ffc8e5a2-a173-4b3d-8627-6ab73d6b234d:bee7d48e-d05f-43ec-aac9-ba404d6a55e3

/** AK old PC mediator test:

--enforce-mediator=10.0.2.41:3345 --listen --echo --cloud-credentials=93e0467f-3145-41a8-8ebc-7f3c95e2ccf0:32cfaaf7-19fe-4bb2-a06d-4b6bac489757 --server-id=xxx
--enforce-mediator=10.0.2.41:3345 --connect --echo --target=xxx.93e0467f-3145-41a8-8ebc-7f3c95e2ccf0 --bytes-to-recieve=1m --total-connections=5 --max-concurrent-connections=5
*/
