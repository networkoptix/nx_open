/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ONVIF

#include "soapserver.h"

#include <sstream>

#include <utils/common/log.h>
#include <utils/common/systemerror.h>


static const unsigned int ERROR_SKIP_TIMEOUT_MS = 500;
static const int SOAP_CONNECTION_ACCEPT_TIMEOUT = 1;    //one second

static QnSoapServer* QnSoapServer_instance = nullptr;

QnSoapServer::QnSoapServer( unsigned int port, const char* path )
:
    m_port( port ),
    m_path( path ),
    m_terminated( false ),
    m_initialized( false )
{
    setObjectName(QLatin1String("QnSoapServer"));

    assert( QnSoapServer_instance == nullptr );
    QnSoapServer_instance = this;
}

QnSoapServer::~QnSoapServer()
{
    QnSoapServer_instance = nullptr;
    stop();
    m_service.soap_force_close_socket();
}

void QnSoapServer::pleaseStop()
{
    m_terminated = true;
}

bool QnSoapServer::bind()
{
    strcpy(m_service.soap->endpoint, m_path.c_str());
    strcpy(m_service.soap->path, m_path.c_str());

    m_service.soap->accept_timeout = SOAP_CONNECTION_ACCEPT_TIMEOUT;
    m_service.soap->imode |= SOAP_XML_IGNORENS;

    int m = soap_bind( m_service.soap, NULL, m_port, 100 );
    if (m < 0)
    {
        std::ostringstream ss;
        soap_stream_fault(m_service.soap, ss);
        NX_LOG(lit("Error binding soap server to local port. %1").arg(QString::fromStdString(ss.str())), cl_logWARNING);
        return false;
    }

    sockaddr_in addr;
    const unsigned int addr_len = sizeof(addr);
    if (getsockname(m_service.soap->master, (sockaddr *)&addr, (socklen_t *)&addr_len) < 0)
    {
        NX_LOG(lit("Error reading soap server port %1").arg(SystemError::getLastOSErrorText()), cl_logWARNING);
        soap_destroy(m_service.soap);
        soap_end(m_service.soap);
        soap_done(m_service.soap);
        return false;
    }
    m_port = ntohs(addr.sin_port);
    m_initialized = true;

    NX_LOG(lit("Successfully bound soap server to local port %1").arg(m_port), cl_logDEBUG1);
    return true;
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

QnSoapServer* QnSoapServer::instance()
{
    return QnSoapServer_instance;
}

void QnSoapServer::run()
{
    initSystemThreadId();

    if( !m_initialized && !bind() )
        return;

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
