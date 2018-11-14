#include "load_emulator.h"

#include <iostream>

#include <nx/network/socket_global.h>
#include <nx/utils/argument_parser.h>

#include <utils/common/command_line_parser.h>

#include "fetch_data.h"
#include "generate_data.h"
#include "load_tests.h"

int main(int argc, char* argv[])
{
    nx::network::SocketGlobals::InitGuard socketGlobals;

    QString cdbUrl;
    QString accountEmail;
    QString accountPassword;
    bool loadMode = false;
    int connectionCount = 100;
    int testSystemsToGenerate = -1;
    int testRequestCount = 0;
    QString fetchRequest;
    QString maxDelayBeforeConnectStr;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&cdbUrl, "--cdb", "-c", "Cloud db url");
    commandLineParser.addParameter(&accountEmail, "--user", "-u", "Account login");
    commandLineParser.addParameter(&accountPassword, "--password", "-p", "Account password");
    commandLineParser.addParameter(&loadMode, "--load", "-l", "Establish many connections");
    commandLineParser.addParameter(&connectionCount, "--connections", "", "Test connection count");
    commandLineParser.addParameter(
        &maxDelayBeforeConnectStr,
        "--max-delay", "",
        "Maximum delay before starting connection. By default, no delay");
    commandLineParser.addParameter(
        &testSystemsToGenerate, "--generate-systems", "-s", "Number of random systems to generate");
    commandLineParser.addParameter(&fetchRequest, "--fetch", nullptr, "Fetch data. Values: systems");
    commandLineParser.addParameter(&testRequestCount, "--api-requests", nullptr, "Make api requests");

    commandLineParser.parse(argc, (const char**)argv, stderr);

    if (cdbUrl.isEmpty() || accountEmail.isEmpty() || accountPassword.isEmpty())
    {
        std::cerr<<"Following arguments are required: "
            "Cloud db url, Account login, Account password"<<std::endl;
        return 1;
    }

    const auto maxDelayBeforeConnect = nx::utils::parseTimerDuration(maxDelayBeforeConnectStr);

    if (loadMode)
    {
        return nx::cloud::db::client::establishManyConnections(
            cdbUrl.toStdString(),
            accountEmail.toStdString(),
            accountPassword.toStdString(),
            connectionCount,
            maxDelayBeforeConnect);
    }

    if (testSystemsToGenerate > 0)
    {
        return nx::cloud::db::client::generateSystems(
            cdbUrl.toStdString(),
            accountEmail.toStdString(),
            accountPassword.toStdString(),
            testSystemsToGenerate);
    }

    if (!fetchRequest.isEmpty())
    {
        return nx::cloud::db::client::fetchData(
            cdbUrl.toStdString(),
            accountEmail.toStdString(),
            accountPassword.toStdString(),
            fetchRequest.toStdString());
    }

    if (testRequestCount > 0)
    {
        return nx::cloud::db::client::makeApiRequests(
            cdbUrl.toStdString(),
            accountEmail.toStdString(),
            accountPassword.toStdString(),
            testRequestCount,
            maxDelayBeforeConnect);
    }

    return 0;
}
