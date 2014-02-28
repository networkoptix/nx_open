/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ONVIF

#include "soapserver.h"

#include <sstream>

#include <utils/common/log.h>


QnSoapServer::QnSoapServer()
:
    m_port( 0 ),
    m_terminated( false ),
    m_initialized( false )
{
    setObjectName( QLatin1String("QnSoapServer") );
}

QnSoapServer::QnSoapServer( unsigned int port, const char* path )
:
    m_port( 0 ),
    m_terminated( false ),
    m_initialized( false )
{
    initialize( port, path );
    setObjectName( QLatin1String("QnSoapServer") );
}

QnSoapServer::~QnSoapServer()
{
    stop();
    m_service.soap_force_close_socket();
}

void QnSoapServer::pleaseStop()
{
    m_terminated = true;
}

void QnSoapServer::initialize( unsigned int port, const char* path )
{
    m_port = port;
    m_path = path;
    m_initialized = true;
}

bool QnSoapServer::initialized() const
{
    return m_initialized;
}

unsigned int QnSoapServer::port() const
{
    return m_port;
}

QString QnSoapServer::path() const
{
    return QString::fromStdString(m_path);
}

OnvifNotificationConsumer* QnSoapServer::getService()
{
    return &m_service;
}

const OnvifNotificationConsumer* QnSoapServer::getService() const
{
    return &m_service;
}

static QnSoapServer* globalInstance = NULL;

void QnSoapServer::initStaticInstance( QnSoapServer* inst )
{
    globalInstance = inst;
}

QnSoapServer* QnSoapServer::instance()
{
    return globalInstance;
}

static const unsigned int ERROR_SKIP_TIMEOUT_MS = 500;

static const int SOAP_CONNECTION_ACCEPT_TIMEOUT = 1;    //one second

void QnSoapServer::run()
{
    initSystemThreadId();
    strcpy( m_service.soap->endpoint, m_path.c_str() );
    strcpy( m_service.soap->path, m_path.c_str() );

    m_service.soap->accept_timeout = SOAP_CONNECTION_ACCEPT_TIMEOUT;
    m_service.soap->imode |= SOAP_XML_IGNORENS;

    int m = soap_bind( m_service.soap, NULL, m_port, 100 ); 
    if( m < 0 )
    {
        std::ostringstream ss;
        soap_stream_fault( m_service.soap, ss );
        NX_LOG( lit("Error binding soap server to port %1. %2").arg(m_port).arg(QString::fromStdString(ss.str())), cl_logDEBUG1 );
        return;
    }

    while( !m_terminated )
    {
        SOAP_SOCKET s = soap_accept( m_service.soap );
        if( s == SOAP_INVALID_SOCKET )
        {
            //error or timeout
            if( m_terminated )
                break;
            //std::ostringstream ss;
            //soap_stream_fault( m_service.soap, ss );
            //NX_LOG( lit("Error accepting soap connection. %1").arg(QString::fromStdString(ss.str())), cl_logDEBUG1 );
            msleep( ERROR_SKIP_TIMEOUT_MS );
            continue;
        }
        // when using SSL: ssl_accept(&abc); 

        //if( soap_begin_serve(m_service.soap) == SOAP_OK ) // available in 2.8.2 and later 
        if( m_service.serve() == SOAP_OK )
        {
            //success
        //}
        //else if( m_service.serve() == SOAP_NO_METHOD )
        //{ 
            //TODO/IMPL selecting next 
            //soap_copy_stream( &uvw, &abc );
            //if( uvw.dispatch() == SOAP_NO_METHOD )
            //{ 
            //    soap_copy_stream(&xyz, &uvw); 
            //    if( xyz.dispatch() )
            //    { 
            //        soap_send_fault(&xyz); // send fault to client
            //        xyz.soap_stream_fault(std::cerr); 
            //    } 
            //    soap_free_stream(&xyz); // free the copy 
            //    xyz.destroy(); 
            //} 
            //else
            //{ 
            //    soap_send_fault(&uvw); // send fault to client 
            //    uvw.soap_stream_fault(std::cerr); 
            //} 
            //soap_free_stream(&uvw); // free the copy 
            //uvw.destroy(); 
        } 
        else
        {
            if( m_terminated )
                break;
            std::ostringstream ss;
            soap_stream_fault( m_service.soap, ss );
            NX_LOG( lit("Error serving soap request %1").arg(QString::fromStdString(ss.str())), cl_logDEBUG1 );
        }
        soap_destroy( m_service.soap );
        soap_end( m_service.soap );
    }

    soap_destroy( m_service.soap );
    soap_end( m_service.soap );
    soap_done( m_service.soap );
}

#endif //ENABLE_ONVIF
