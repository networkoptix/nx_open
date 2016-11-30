#include <QtCore/QCoreApplication>

#include <nx/network/socket_global.h>
#include <nx/utils/test_support/run_test.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    return nx::utils::runTest(argc, argv);
}
