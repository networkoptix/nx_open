/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <utils/common/log.h>
#include <utils/network/http/httpclient.h>
#include <utils/network/http/auth_tools.h>


int main( int argc, char **argv )
{
    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
