#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>
#include <qcoreapplication.h>
#include <QCommandLineParser>
#include <nx/network/socket_global.h>

int main(int argc, char **argv)
{
	nx::network::SocketGlobals::InitGuard sgGuard;
    QCoreApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
