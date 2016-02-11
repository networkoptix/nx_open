/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>

#include <test_support/socket_globals_holder.h>


int main( int argc, char **argv )
{
    SocketGlobalsHolder socketGlobalsInstance;
    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 0; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg.find("--log=") == 0)
            QnLog::initLog(QString::fromStdString(arg.substr(6)));
    }

    const int result = RUN_ALL_TESTS();
    return result;
}
