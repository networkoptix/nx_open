/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtConcurrent>

#include "transaction/transaction.h"


namespace ec2
{
    Ec2DirectConnectionFactory::Ec2DirectConnectionFactory()
    {
    }

    Ec2DirectConnectionFactory::~Ec2DirectConnectionFactory()
    {
    }

    //!Implementation of AbstractECConnectionFactory::testConnectionAsync
    ReqID Ec2DirectConnectionFactory::testConnectionAsync( const ECAddress& /*addr*/, impl::SimpleHandlerPtr handler )
    {
        QtConcurrent::run( std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, ec2::ErrorCode::ok ) );
        return 0;
    }

    //!Implementation of AbstractECConnectionFactory::connectAsync
    ReqID Ec2DirectConnectionFactory::connectAsync( const ECAddress& /*addr*/, impl::ConnectHandlerPtr handler )
    {
        {
            std::lock_guard<std::mutex> lk( m_mutex );
            if( !m_connection )
                m_connection.reset( new Ec2DirectConnection() );
        }
        QtConcurrent::run( std::bind( std::mem_fn( &impl::ConnectHandler::done ), handler, ec2::ErrorCode::ok, m_connection ) );
        return 0;
    }

    //template<class T2>
    ////void deserialize(typename std::enable_if<std::is_integral<T2>::value >::type& field, BinaryStream<T>* binStream)
    ////void deserialize(typename std::enable_if<std::is_enum<T2>::value >::type& field, BinaryStream<T>* binStream)
    ////typename std::enable_if<std::is_enum<T2>::value, void>::type deserialize(T2& field, BinaryStream<T>* binStream)
    //void deserialize(typename std::enable_if<std::is_enum<T2>::value, T2>::type& field)
    //{
    //    HER;
    //}

    //template<class T> void deserialize( T& field ) {}

    //enum TEST
    //{
    //    t1,
    //    t2,
    //    t3
    //};

    //int test()
    //{
    //    TEST t;
    //    deserialize<TEST>( t );
    //}


}
