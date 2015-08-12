/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <utils/common/log.h>

int main( int argc, char **argv )
{
    QnLog::initLog("DEBUG2");

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
