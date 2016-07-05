/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <utils/common/command_line_parser.h>

int main(int argc, char **argv)
{
    nx::network::SocketGlobals::InitGuard sgGuard;
    ::testing::InitGoogleTest(&argc, argv);

    nx::utils::ArgumentParser args(argc, argv);
    QnLog::applyArguments(args);
    nx::network::SocketGlobals::applyArguments(args);

    const int result = RUN_ALL_TESTS();
    return result;
}
