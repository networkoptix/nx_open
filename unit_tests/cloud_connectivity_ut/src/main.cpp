/**********************************************************
* Dec 29, 2015
* akolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

int main(int argc, char **argv)
{
    nx::network::SocketGlobalsHolder socketGlobalsInstance;
    ::testing::InitGoogleMock(&argc, argv);

    for (int i = 0; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg.find("--log-level=") == 0)
        {
            QnLog::initLog(QString::fromStdString(arg.substr(12)));

            // do not allow mediator process reinit log engine
            QnLog::s_disableLogConfiguration = true;
        }
    }

    const int result = RUN_ALL_TESTS();
    return result;
}
