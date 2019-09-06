#include "load_emulator.h"

#include <iostream>
#include <regex>

#include <nx/network/socket_global.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log_initializer.h>

#include "fetch_data.h"
#include "generate_data.h"
#include "load_tests.h"

void printHelp()
{
    std::cout << R"(
Common arguments:
-c, --cdb               CloudDb url. E.g., https://cloud-test.hdw.mx
-u, --user              Account login
-p, --password          Account password
-h, --help              This help message

Data generation mode:
-s, --generate-systems= Number of random systems to generate
Creates active systems with name load_test_system_{some_id}

)";

    nx::cloud::db::client::LoadTest::printHelp(std::cout);

    std::cout <<
        "--fetch                 Fetch data. Values: systems. TODO" <<
        std::endl <<
        std::endl;
}

int main(int argc, const char* argv[])
{
    nx::utils::ArgumentParser args(argc, argv);
    if (args.get("help"))
    {
        printHelp();
        return 0;
    }

    QString cdbUrl = args.get("cdb", "c").value_or(QString());
    QString accountEmail = args.get("user", "u").value_or(QString());
    QString accountPassword = args.get("password", "p").value_or(QString());
    bool loadMode = static_cast<bool>(args.get("load", "l"));
    int testSystemsToGenerate = args.get<int>("generate-systems", "s").value_or(-1);
    int testRequestCount = args.get<int>("api-requests").value_or(0);
    QString fetchRequest = args.get("fetch").value_or(QString());

    nx::utils::log::initializeGlobally(args);

    nx::network::SocketGlobals::InitGuard socketGlobals;
    nx::network::SocketGlobals::applyArguments(args);

    if (cdbUrl.isEmpty() || accountEmail.isEmpty() || accountPassword.isEmpty())
    {
        std::cerr<<"Following arguments are required: "
            "Cloud db url, Account login, Account password"<<std::endl;
        return 1;
    }

    if (loadMode)
    {
        return nx::cloud::db::client::LoadTest::establishManyConnections(
            cdbUrl.toStdString(),
            accountEmail.toStdString(),
            accountPassword.toStdString(),
            args);
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
        return nx::cloud::db::client::LoadTest::makeApiRequests(
            cdbUrl.toStdString(),
            accountEmail.toStdString(),
            accountPassword.toStdString(),
            testRequestCount);
    }

    std::cerr << "Nothing to do. Specify some command" << std::endl;

    return 0;
}
