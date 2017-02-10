#include "load_emulator.h"

#include <iostream>

#include <nx/utils/argument_parser.h>

#include <utils/common/command_line_parser.h>

namespace nx {
namespace cdb {

int establishManyConnections()
{
    auto loadEmulator = std::make_unique<test::LoadEmulator>(
        "https://cloud-dev.hdw.mx",
        "akolesnikov@networkoptix.com",
        "123");

    loadEmulator->setTransactionConnectionCount(1000);
    loadEmulator->start();

    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}

} // namespace cdb
} // namespace nx

int main(int argc, char* argv[])
{
    QString accountEmail;
    QString accountPassword;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&accountEmail, "--user", "-u", "Account login");
    commandLineParser.addParameter(&accountPassword, "--password", "-p", "Account password");

    commandLineParser.parse(argc, (const char**)argv, stderr);

    return 0;
}
