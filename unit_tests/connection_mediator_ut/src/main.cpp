/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>

#include "socket_globals_holder.h"


int main( int argc, char **argv )
{
    SocketGlobalsHolder socketGlobalsInstance;
    //QnLog::initLog("DEBUG2");

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
