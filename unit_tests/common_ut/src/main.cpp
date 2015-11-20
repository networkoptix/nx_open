/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <utils/common/log.h>
#include <nx/network/socket_factory.cpp>

int main( int argc, char **argv )
{
    //QnLog::initLog("DEBUG2");

    ::testing::InitGoogleTest( &argc, argv );
    for( int i = 0; i < argc; ++i )
    {
        std::string arg( argv[ i ] );
        if( arg == "--enforce-socket=tcp" )
            SocketFactory::enforceStreamSocketType( SocketFactory::SocketType::Tcp );
        else
        if( arg == "--enforce-socket=udt" )
            SocketFactory::enforceStreamSocketType( SocketFactory::SocketType::Udt );
    }

    return RUN_ALL_TESTS();
}
