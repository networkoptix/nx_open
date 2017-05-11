#include <QCoreApplication>

#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    return nx::network::test::runTest(argc, argv);
}
