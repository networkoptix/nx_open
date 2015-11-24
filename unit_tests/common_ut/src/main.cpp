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
#if 1
    cl_log.create(
        "c:\\tmp\\common_ut.log",
        50*1024*1024,
        10,
        cl_logDEBUG2);
#endif

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
