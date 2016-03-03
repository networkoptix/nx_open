/**********************************************************
* Mar 3, 2016
* a.kolesnikov
***********************************************************/

#include <iostream>

#include <utils/common/command_line_parser.h>

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/socket_test_helper.h>


void printHelp();
int runInListenMode(const std::multimap<QString, QString>& args);
int runInConnectMode(const std::multimap<QString, QString>& args);

int main(int argc, char* argv[])
{
    std::multimap<QString, QString> args;
    parseCmdArgs(argc, argv, &args);
    if (args.find("help") != args.end())
    {
        printHelp();
        return 0;
    }

    //reading mode
    if (args.find("listen") != args.end())
        return runInListenMode(args);

    if (args.find("connect") != args.end())
        return runInConnectMode(args);

    std::cerr<<"error. Unknown mode"<<std::endl;
    printHelp();

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

    SocketGlobals::InitGuard socketGlobalsGuard;

    SocketGlobals::mediatorConnector().setSystemCredentials(
        nx::hpm::api::SystemCredentials(
            credentials[0].toUtf8(),
            QnUuid::createUuid().toByteArray(),
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
    std::cout << "listening on mediator" << std::endl;

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
        std::cout<<"Enter \"exit\" to quit >> ";
        std::string s;
        std::cin>>s;
        if (s == "exit")
            break;
    }

    server.pleaseStop();
    server.join();

    return 0;
}

int runInConnectMode(const std::multimap<QString, QString>& args)
{
    return 1;
}

void printHelp()
{
    std::cout << 
        "\n"
        "Listen mode:\n"
        "  --listen                         Enable listen mode\n"
        "  --cloud-credentials={system_id}:{authentication_key}"
        "                                   Specify credentials to use to connect to mediator\n"
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
        << std::endl;
}
