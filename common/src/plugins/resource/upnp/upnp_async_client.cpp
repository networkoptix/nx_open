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

// TODO: move parsers to separate file
class UpnpMessageHandler
        : public QXmlDefaultHandler
{
public:
    virtual bool startDocument() override
    {
        m_message = UpnpAsyncClient::Message();
        m_awaitedValue = 0;
        return true;
    }

    virtual bool endDocument() override { return true; }

    virtual bool startElement( const QString& /*namespaceURI*/,
                               const QString& /*localName*/,
                               const QString& qName,
                               const QXmlAttributes& /*attrs*/ ) override
    {
        if( qName == lit("xml") || qName == lit("s:Envelope") )
            return true; // TODO: check attrs

        m_awaitedValue = &m_message.params[qName];
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

protected:
    UpnpAsyncClient::Message m_message;
    QString* m_awaitedValue;
};

class UpnpSuccessHandler
        : public UpnpMessageHandler
{
    typedef UpnpMessageHandler super;

public:
    virtual bool startDocument() override
    {
        return super::startDocument();
    }

    virtual bool startElement( const QString& namespaceURI,
                               const QString& localName,
                               const QString& qName,
                               const QXmlAttributes& attrs ) override
    {
        if( qName == lit("s:Body") )
            return true;

        if( qName.startsWith( lit("u:") ) )
        {
            m_message.action = localName;
            m_message.service = fromUpnpUrn( namespaceURI, lit("service") );
            return true;
        }

        return super::startElement( namespaceURI, localName, qName, attrs );
    }

private:
    bool m_firstInBody;
};

class UpnpFailureHandler
        : public UpnpMessageHandler
{
    typedef UpnpMessageHandler super;

public:
    virtual bool startElement( const QString& namespaceURI,
                               const QString& localName,
                               const QString& qName,
                               const QXmlAttributes& attrs ) override
    {
        if( qName == lit("s:Fault") || qName == lit("s:UPnpError") || qName == lit("detail") )
            return true;

        return super::startElement( namespaceURI, localName, qName, attrs );
    }
};

bool UpnpAsyncClient::Message::isOk() const
{
    return !action.isEmpty() && !service.isEmpty();
}

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
        {
            QMutexLocker lk(&m_mutex);
            m_httpClients.erase( ptr );
        }

        if (auto resp = ptr->response())
        {
            const auto status = resp->statusLine.statusCode;
            const bool success = (status >= 200 && status < 300);

            std::unique_ptr<UpnpMessageHandler> xmlHandler;
            if( success )   xmlHandler.reset( new UpnpSuccessHandler );
            else            xmlHandler.reset( new UpnpFailureHandler );

            QXmlSimpleReader xmlReader;
            xmlReader.setContentHandler( xmlHandler.get() );
            xmlReader.setErrorHandler( xmlHandler.get() );

            QXmlInputSource input;
            input.setData( ptr->fetchMessageBodyBuffer() );
            if( xmlReader.parse( &input ) )
            {
                callback( xmlHandler->message() );
                return;
            }
        }

        // TODO: retry?
    };

    auto httpClient = std::make_shared< nx_http::AsyncHttpClient >();
    httpClient->addAdditionalHeader( "SOAPAction", action.toUtf8() );
    QObject::connect( httpClient.get(), &nx_http::AsyncHttpClient::done,
                      httpClient.get(), complete, Qt::DirectConnection );

    if( httpClient->doPost( url, "text/xml", request.toUtf8() ) )
    {
        QMutexLocker lk(&m_mutex);
        m_httpClients.insert( std::move( httpClient ) );
        return true;
    }

    return false;
}

const QString UpnpAsyncClient::CLIENT_ID        = lit( "NX UpnpAsyncClient" );
const QString UpnpAsyncClient::INTERNAL_GATEWAY = lit( "InternetGatewayDevice" );
const QString UpnpAsyncClient::WAN_IP           = lit( "WANIpConnection" );

static const QString GET_EXTERNAL_IP    = lit( "GetExternalIPAddress" );
static const QString ADD_PORT_MAPPING   = lit( "AddPortMapping" );
static const QString DELETE_PORT_MAPPING   = lit( "DeletePortMapping" );
static const QString GET_GEN_PORT_MAPPING   = lit( "GetGenericPortMappingEntry" );
static const QString GET_SPEC_PORT_MAPPING   = lit( "GetSpecificPortMappingEntry" );

static const QString MAP_INDEX      = lit("NewPortMappingIndex");
static const QString EXTERNAL_IP    = lit("NewExternalIPAddress");
static const QString EXTERNAL_PORT  = lit("NewExternalPort");
static const QString PROTOCOL       = lit("NewProtocol");
static const QString INTERNAL_PORT  = lit("NewInternalPort");
static const QString INTERNAL_IP    = lit("NewInternalClient");
static const QString ENABLED        = lit("NewEnabled");
static const QString DESCRIPTION    = lit("NewPortMappingDescription");
static const QString DURATION       = lit("NewLeaseDuration");

bool UpnpAsyncClient::externalIp( const QUrl& url,
                                  const std::function< void( const HostAddress& ) >& callback )
{
    UpnpAsyncClient::Message request = { GET_EXTERNAL_IP, WAN_IP };
    return doUpnp(  url, request, [callback]( const Message& response ) {
        callback( response.getParam( EXTERNAL_IP ) );
    } );
}

bool UpnpAsyncClient::addMapping(
        const QUrl& url, const HostAddress& internalIp, quint16 internalPort,
        quint16 externalPort, const QString& protocol,
        const std::function< void( bool ) >& callback )
{
    UpnpAsyncClient::Message request;
    request.action = ADD_PORT_MAPPING;
    request.service = WAN_IP;

    request.params[ EXTERNAL_PORT ] = QString::number( externalPort );
    request.params[ PROTOCOL ]      = protocol;
    request.params[ INTERNAL_PORT ] = QString::number( internalPort );
    request.params[ INTERNAL_IP ]   = internalIp.toString();
    request.params[ ENABLED]        = QString::number( 1 );
    request.params[ DESCRIPTION ]   = CLIENT_ID;
    request.params[ DURATION ]      = QString::number( 0 );

    return doUpnp( url, request, [ callback ]( const Message& response ) { 
        callback( response.isOk() );
    } );
}

bool UpnpAsyncClient::deleteMapping(
        const QUrl& url, quint16 externalPort, const QString& protocol,
        const std::function< void( bool ) >& callback )
{
    UpnpAsyncClient::Message request;
    request.action = DELETE_PORT_MAPPING;
    request.service = WAN_IP;
    request.params[ EXTERNAL_PORT ] = QString::number( externalPort );
    request.params[ PROTOCOL ]      = protocol;

    return doUpnp( url, request, [ callback ]( const Message& response ) {
        callback( response.isOk() );
    } );
}

bool UpnpAsyncClient::getMapping( const QUrl& url, quint32 index,
                                  const MappingInfoCallback& callback )
{
    UpnpAsyncClient::Message request;
    request.action = DELETE_PORT_MAPPING;
    request.service = WAN_IP;
    request.params[ MAP_INDEX ] = QString::number( index );

    return doUpnp( url, request, [ callback ]( const Message& response )
    {
        if( response.isOk() )
        {
            const auto internalIp = response.getParam( INTERNAL_IP );
            const auto internalPort = response.getParam( INTERNAL_PORT ).toUShort();
            const auto externalPort = response.getParam( EXTERNAL_PORT ).toUShort();
            const auto protocol = response.getParam( PROTOCOL );
            callback( internalIp, internalPort, externalPort, protocol );
        }
        else
        {
            static const QString none;
            callback( none, 0, 0, none );

        }
    } );
}

bool UpnpAsyncClient::getMapping(
        const QUrl& url, quint16 externalPort, const QString& protocol,
        const MappingInfoCallback& callback )
{
    UpnpAsyncClient::Message request;
    request.action = GET_SPEC_PORT_MAPPING;
    request.service = WAN_IP;
    request.params[ EXTERNAL_PORT ] = QString::number( externalPort );
    request.params[ PROTOCOL ]      = protocol;

    return doUpnp( url, request,
                   [ callback, externalPort, protocol ]( const Message& response )
    {
        if( response.isOk() )
        {
            const auto internalIp = response.getParam( INTERNAL_IP );
            const auto internalPort = response.getParam( INTERNAL_PORT ).toUShort();
            callback( internalIp, internalPort, externalPort, protocol );
        }
        else
        {
            static const QString none;
            callback( none, 0, externalPort, protocol );

        }
    } );
}
