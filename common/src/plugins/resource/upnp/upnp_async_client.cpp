#include "upnp_async_client.h"

#include "upnp_device_description.h"

static const QString SOAP_REQUEST = QLatin1String(
    "<?xml version=\"1.0\" ?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
               " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
            "<u:%1 xmlns:u=\"%2\">"
                "%3" // params
            "</u:%1>"
        "</s:Body>"
    "</s:Envelope>"
);

class UpnpMessageHandler
    : public QXmlDefaultHandler
{
public:
    virtual bool startDocument() override
    {
        m_message = UpnpAsyncClient::Message();
        m_firstInBody = false;
        m_awaitedValue = 0;
        return true;
    }

    virtual bool endDocument() override { return true; }

    virtual bool startElement( const QString& /*namespaceURI*/,
                               const QString& /*localName*/,
                               const QString& qName,
                               const QXmlAttributes& attrs ) override
    {
        if( qName == lit("xml") || qName == lit("s:Envelope") )
            static_cast<void>(0); // TODO: check fields
        else
        if( qName == lit("s:Body") )
            m_firstInBody = true;
        else
        {
            if (m_firstInBody)
            {
                m_message.action = qName.split( lit(":") )[1];
                m_message.service = fromUpnpUrn( attrs.value( lit("xmlns:u") ), lit("service") );
                m_firstInBody = false;
            }
            else
                m_awaitedValue = &m_message.params[qName];
        }

        return true;
    }

    virtual bool endElement( const QString& namespaceURI,
                             const QString& localName,
                             const QString& qName ) override
    {
        m_awaitedValue = 0;
        return true;
    }

    virtual bool characters( const QString& ch ) override
    {
        if (m_awaitedValue)
            *m_awaitedValue = ch;

        return true;
    }

    const UpnpAsyncClient::Message& message() const { return m_message; }

private:
    UpnpAsyncClient::Message m_message;
    bool m_firstInBody;
    QString* m_awaitedValue;
};

const QString& UpnpAsyncClient::Message::getParam( const QString& key ) const
{
    static const QString none;
    const auto it = params.find( key );
    return ( it == params.end() ) ? none : it->second;
}

bool UpnpAsyncClient::doUpnp( const QUrl& url, const Message& message,
                              const std::function< void( const Message& )>& callback )
{
    const auto service = toUpnpUrn( message.service, lit("service") );
    const auto action = lit( "\"%1#%2\"" ).arg( service ).arg( message.action );

    QStringList params;
    for( const auto& p : message.params )
        params.push_back( lit( "<%1>%2</%1>" ).arg( p.first ).arg( p.second ) );

    const auto request = SOAP_REQUEST.arg( message.action ).arg( service )
                                     .arg( params.join( lit("") ) );

    const auto complete = [this, callback]( const nx_http::AsyncHttpClientPtr& ptr )
    {
        m_httpClients.erase( ptr );
        if( ptr->hasRequestSuccesed() )
        {
            UpnpMessageHandler xmlHandler;
            QXmlSimpleReader xmlReader;
            xmlReader.setContentHandler( &xmlHandler );
            xmlReader.setErrorHandler( &xmlHandler );

            QXmlInputSource input;
            input.setData( ptr->fetchMessageBodyBuffer() );
            if( xmlReader.parse( &input ) )
            {
                callback( xmlHandler.message() );
                return;
            }
        }

        callback( Message() ); // error
    };

    auto httpClient = std::make_shared< nx_http::AsyncHttpClient >();
    httpClient->addAdditionalHeader( "SOAPAction", action.toUtf8() );
    QObject::connect( httpClient.get(), &nx_http::AsyncHttpClient::done,
                      httpClient.get(), complete, Qt::DirectConnection );

    if( httpClient->doPost( url, "text/xml", request.toUtf8() ) )
    {
        m_httpClients.insert( std::move( httpClient ) );
        return true;
    }

    return false;
}

static const QString WAN_IP = lit( "WANIpConnection" );
static const QString GET_EXTERNAL_IP = lit( "GetExternalIPAddress" );
static const QString EXTERNAL_IP = lit("NewExternalIPAddress");

bool UpnpAsyncClient::externalIp( const QUrl& url,
                                  const std::function< void( const QString& ) >& callback )
{
    UpnpAsyncClient::Message request = { GET_EXTERNAL_IP, WAN_IP };
    return doUpnp(  url, request, [callback]( const Message& response ) {
        callback( response.getParam( EXTERNAL_IP ) );
    } );
}

/*
bool UpnpAsyncClient::addMapping( const QUrl& url, const Mapping& mapping,
                                  const std::function< Mapping >& callback )
{
    UpnpAsyncClient::Message request;
    request.action = lit( "AddPortMapping" );
    request.service = lit( "WANIpConnection" );

    request.params[ lit( "NewExternalPort") ] = QString::number( mapping.externalPort );
    request.params[ lit( "NewProtocol") ] = mapping.protocol;
    request.params[ lit( "NewInternalPort") ] = QString::number( mapping.internalPort );
    request.params[ lit( "NewInternalClient") ] = mapping.internalIp;
    request.params[ lit( "NewEnabled") ] = QString::number( 1 );
    request.params[ lit( "NewPortMappingDescription") ] = lit( "NX UpnpAsyncClient" );
    request.params[ lit( "NewLeaseDuration") ] = QString::number( 0 );

    return doUpnp(  url, request, [callback]( const Message& response ) {
        if( response.action.isNull )
        {
            callback
            return;
        }
    } );
}
*/
