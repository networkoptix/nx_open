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
        std::string arg(argv[i]);
        if (arg == "--enforce-socket=tcp")
            SocketFactory::enforceStreamSocketType(SocketFactory::SocketType::Tcp);
        else
        if (arg == "--enforce-socket=udt")
            SocketFactory::enforceStreamSocketType(SocketFactory::SocketType::Udt);
        else
        if (arg.find("--log=") == 0)
            QnLog::initLog(QString::fromStdString(arg.substr(6)));
    }

    return RUN_ALL_TESTS();
}
