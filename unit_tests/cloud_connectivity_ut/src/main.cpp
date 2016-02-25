/**********************************************************
* Dec 29, 2015
* akolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

#include <libconnection_mediator/src/test_support/socket_globals_holder.h>


int main(int argc, char **argv)
{
    SocketGlobalsHolder socketGlobalsInstance;
    ::testing::InitGoogleMock(&argc, argv);

    for (int i = 0; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg.find("--log=") == 0)
            QnLog::initLog(QString::fromStdString(arg.substr(6)));
    }

    const int result = RUN_ALL_TESTS();
    return result;
}
