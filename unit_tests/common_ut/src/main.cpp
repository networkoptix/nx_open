/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <utils/common/log.h>
#include <utils/network/socket_factory.cpp>

int main( int argc, char **argv )
{
    //QnLog::initLog("DEBUG2");

    ::testing::InitGoogleTest( &argc, argv );
    for( int i = 0; i < argc; ++i )
    {
        std::string arg( argv[ i ] );
        if( arg == "--enforce-socket-tcp" ) {
            SocketFactory::enforceStreamSocketType( SocketFactory::SocketType::Tcp );
            std::cout << "### Enforced socket type: TCP ###" << std::endl;
            continue;
        }
        if( arg == "--enforce-socket-udt" ) {
            SocketFactory::enforceStreamSocketType( SocketFactory::SocketType::Udt );
            std::cout << "### Enforced socket type: UDT ###" << std::endl;
            continue;
        }
    }

    return RUN_ALL_TESTS();
}
