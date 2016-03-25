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

    test::RandomDataTcpServer server(1024*1024);
    server.setServerSocket(std::move(serverSock));
    if (!server.start())
    {
        std::cerr << "error. Failed to start accepting connections. Reason: " <<
            SystemError::getLastOSErrorText().toStdString() << std::endl;
        return 1;
    }

    for (;;)
    {
        std::cout<<
            "Enter \"exit\" to exit... \n"
            "\n"
            ">> ";
        std::string s;
        std::cin>>s;
        if (s == "exit")
            break;
    }

    server.pleaseStop();
    server.join();

    return 0;
}

const int kDefaultTotalConnections = 100;
const int kDefaultMaxConcurrentConnections = 1;
const int kDefaultBytesToSend = 100000;

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

    int bytesToSend = kDefaultBytesToSend;
    readArg(args, "bytes-to-send", &bytesToSend);

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
        bytesToSend,
        totalConnections);

    std::promise<void> finishedPromise;
    connectionsGenerator.setOnFinishedHandler(
        [&finishedPromise]{ finishedPromise.set_value(); });

    const auto startTime = std::chrono::steady_clock::now();
    connectionsGenerator.start();
    finishedPromise.get_future().wait();
    const auto testDuration =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime);

    std::cout<<"connect summary: "
        "Total time: "<< testDuration.count() <<"s. "
        "Total connections: "<<connectionsGenerator.totalConnectionsEstablished()<<". "
        "Total bytes sent: "<<connectionsGenerator.totalBytesSent()<<". "
        "Total bytes received: "<<connectionsGenerator.totalBytesReceived()<<". "
        "Total errors: "<<connectionsGenerator.totalErrors().size()<<"."
        <<std::endl;

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

    return 0;
}

void printHelp(int argc, char* argv[])
{
    std::cout << 
        "\n"
        "Listen mode:\n"
        "  --listen                         Enable listen mode\n"
        "  --cloud-credentials={system_id}:{authentication_key}\n"
        "                                   Specify credentials to use to connect to mediator\n"
        "  --server-id={server_id}          Id used when registering on mediator\n"
        "\n"
        "Connect mode:\n"
        "  --connect                        Enable connect mode\n"
        "  --target={endpoint}              Regular or cloud address of server\n"
        "  --total-connections={100}\n"
        "                                   Number of connections to try\n"
        "  --max-concurrent-connections={10}\n"
        "  --bytes-to-send={100000}         This number of bytes is sent to the target\n"
        "                                   before connection termination\n"
        "\n"
        "Http client mode:\n"
        "  --http-client                    Enable Http client mode\n"
        "  --url={http url}                 Url to trigger\n"
        "\n"
        "Common options:\n"
        "  --enforce-mediator={endpoint}   Enforces custom mediator address\n"
        "\n"
        << std::endl;
}
