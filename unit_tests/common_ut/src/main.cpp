/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>

int main(int argc, char **argv)
{
#if 0
    cl_log.create(
        "c:/tmp/common_ut.log",
        50*1024*1024,
        1,
        cl_logDEBUG2);
#endif
	
	nx::network::SocketGlobals::InitGuard sgGuard;
    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 0; i < argc; ++i)
    {
        static const std::string SOCKET("--enforce-socket=");
        static const std::string LOG("--log=");

        std::string arg(argv[i]);
        if (arg.find(SOCKET) == 0)
        {
            SocketFactory::enforceStreamSocketType(
                QString::fromStdString(arg.substr(SOCKET.length())));
        }
        else
        if (arg.find(LOG) == 0)
        {
            QnLog::initLog(QString::fromStdString(arg.substr(LOG.length())));
        }
    }

    return RUN_ALL_TESTS();
}
