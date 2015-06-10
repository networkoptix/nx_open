#include "upnp_async_client.h"

#include "upnp_device_description.h"
#include "utils/common/model_functions.h"
#include "utils/serialization/lexical_functions.h"

namespace nx_upnp {
namespace /* noname */ {

static const auto MESSAGE_BODY_READ_TIMEOUT_MS = 20 * 1000; // 20 sec

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
        m_message = AsyncClient::Message();
        m_awaitedValue = 0;
        return true;
    }

    virtual bool endDocument() override { return true; }

    virtual bool startElement( const QString& /*namespaceURI*/,
                               const QString& localName,
                               const QString& qName,
                               const QXmlAttributes& /*attrs*/ ) override
    {
        if( localName == lit("xml") || localName == lit("Envelope") )
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

    const AsyncClient::Message& message() const { return m_message; }

protected:
    AsyncClient::Message m_message;
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
        if( localName == lit("Body") )
            return true;

        if( qName.startsWith( lit("u:") ) )
        {
            m_message.action = localName;
            m_message.service = fromUpnpUrn( namespaceURI, lit("service") );
            return true;
        }

        return super::startElement( namespaceURI, localName, qName, attrs );
    }
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
        if( localName == lit("Fault") || localName == lit("UPnpError") ||
                localName == lit("detail") )
            return true;

        return super::startElement( namespaceURI, localName, qName, attrs );
    }
};

} // namespace noname

bool AsyncClient::Message::isOk() const
{
    return !action.isEmpty() && !service.isEmpty();
}

static const QString none;
const QString& AsyncClient::Message::getParam( const QString& key ) const
{
    const auto it = params.find( key );
    return ( it == params.end() ) ? none : it->second;
}

bool AsyncClient::doUpnp( const QUrl& url, const Message& message,
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
    httpClient->setMessageBodyReadTimeoutMs( MESSAGE_BODY_READ_TIMEOUT_MS );
    QObject::connect( httpClient.get(), &nx_http::AsyncHttpClient::done,
                      httpClient.get(), complete, Qt::DirectConnection );

    QMutexLocker lk(&m_mutex);
    m_httpClients.insert( httpClient );
    if( !httpClient->doPost( url, "text/xml", request.toUtf8() ) )
    {
        m_httpClients.erase( httpClient );
        return false;
    }

    return true;
}

const QString AsyncClient::CLIENT_ID        = lit( "NX UpnpAsyncClient" );
const QString AsyncClient::INTERNAL_GATEWAY = lit( "InternetGatewayDevice" );
const QString AsyncClient::WAN_IP           = lit( "WANIpConnection" );

static const QString GET_EXTERNAL_IP        = lit( "GetExternalIPAddress" );
static const QString ADD_PORT_MAPPING       = lit( "AddPortMapping" );
static const QString DELETE_PORT_MAPPING    = lit( "DeletePortMapping" );
static const QString GET_GEN_PORT_MAPPING   = lit( "GetGenericPortMappingEntry" );
static const QString GET_SPEC_PORT_MAPPING  = lit( "GetSpecificPortMappingEntry" );

static const QString MAP_INDEX      = lit("NewPortMappingIndex");
static const QString EXTERNAL_IP    = lit("NewExternalIPAddress");
static const QString EXTERNAL_PORT  = lit("NewExternalPort");
static const QString PROTOCOL       = lit("NewProtocol");
static const QString INTERNAL_PORT  = lit("NewInternalPort");
static const QString INTERNAL_IP    = lit("NewInternalClient");
static const QString ENABLED        = lit("NewEnabled");
static const QString DESCRIPTION    = lit("NewPortMappingDescription");
static const QString DURATION       = lit("NewLeaseDuration");

bool AsyncClient::externalIp( const QUrl& url,
                                  const std::function< void( const HostAddress& ) >& callback )
{
    AsyncClient::Message request = { GET_EXTERNAL_IP, WAN_IP };
    return doUpnp( url, request, [callback]( const Message& response ) {
        callback( response.getParam( EXTERNAL_IP ) );
    } );
}

bool AsyncClient::addMapping(
        const QUrl& url, const HostAddress& internalIp, quint16 internalPort,
        quint16 externalPort, Protocol protocol, const QString& description,
        const std::function< void( bool ) >& callback )
{
    AsyncClient::Message request;
    request.action = ADD_PORT_MAPPING;
    request.service = WAN_IP;

    request.params[ EXTERNAL_PORT ] = QString::number( externalPort );
    request.params[ PROTOCOL ]      = QnLexical::serialized( protocol );
    request.params[ INTERNAL_PORT ] = QString::number( internalPort );
    request.params[ INTERNAL_IP ]   = internalIp.toString();
    request.params[ ENABLED]        = QString::number( 1 );
    request.params[ DESCRIPTION ]   = description.isEmpty() ? CLIENT_ID : description;
    request.params[ DURATION ]      = QString::number( 0 );

    return doUpnp( url, request, [ callback ]( const Message& response ) { 
        callback( response.isOk() );
    } );
}

bool AsyncClient::deleteMapping(
        const QUrl& url, quint16 externalPort, Protocol protocol,
        const std::function< void( bool ) >& callback )
{
    AsyncClient::Message request;
    request.action = DELETE_PORT_MAPPING;
    request.service = WAN_IP;
    request.params[ EXTERNAL_PORT ] = QString::number( externalPort );
    request.params[ PROTOCOL ] = QnLexical::serialized( protocol );

    return doUpnp( url, request, [ callback ]( const Message& response ) {
        callback( response.isOk() );
    } );
}

AsyncClient::MappingInfo::MappingInfo(
        const HostAddress& inIp, quint16 inPort, quint16 exPort,
        Protocol prot, const QString& desc )
    : internalIp( inIp ), internalPort( inPort ), externalPort( exPort )
    , protocol( prot ), description( desc )
{
}

bool AsyncClient::MappingInfo::isValid() const
{
    return !( internalIp == HostAddress() ) && internalPort && externalPort;
}

bool AsyncClient::getMapping(
        const QUrl& url, quint32 index,
        const std::function< void( const MappingInfo& ) >& callback )
{
    AsyncClient::Message request;
    request.action = GET_GEN_PORT_MAPPING;
    request.service = WAN_IP;
    request.params[ MAP_INDEX ] = QString::number( index );

    return doUpnp( url, request, [ callback ]( const Message& response )
    {
        if( response.isOk() )
            callback( MappingInfo(
                response.getParam( INTERNAL_IP ),
                response.getParam( INTERNAL_PORT ).toUShort(),
                response.getParam( EXTERNAL_PORT ).toUShort(),
                QnLexical::deserialized< Protocol >( response.getParam( PROTOCOL ) ),
                response.getParam( DESCRIPTION ) ) );
        else
            callback( MappingInfo() );
    } );
}

bool AsyncClient::getMapping(
        const QUrl& url, quint16 externalPort, Protocol protocol,
        const std::function< void( const MappingInfo& ) >& callback )
{
    AsyncClient::Message request;
    request.action = GET_SPEC_PORT_MAPPING;
    request.service = WAN_IP;
    request.params[ EXTERNAL_PORT ] = QString::number( externalPort );
    request.params[ PROTOCOL ] = QnLexical::serialized( protocol );

    return doUpnp( url, request,
                   [ callback, externalPort, protocol ]( const Message& response )
    {
        if( response.isOk() )
            callback( MappingInfo(
                response.getParam( INTERNAL_IP ),
                response.getParam( INTERNAL_PORT ).toUShort(),
                externalPort, protocol,
                response.getParam( DESCRIPTION ) ) );
        else
            callback( MappingInfo() );
    } );
}

void fetchMappingsRecursive(
        AsyncClient* client, const QUrl& url,
        const std::function< void( const AsyncClient::MappingList& ) >& callback,
        const std::shared_ptr< AsyncClient::MappingList >& collected,
        const AsyncClient::MappingInfo& newMap )
{
    if( !newMap.isValid() )
    {
        callback( std::move( *collected ) );
        return;
    }

    collected->push_back( std::move( newMap ) );
    if( !client->getMapping( url, collected->size(),
                             [ client, url, callback, collected ]
                             ( const AsyncClient::MappingInfo& nextMap )
        { fetchMappingsRecursive( client, url, callback, collected, nextMap ); } ) )
    {
        // return what we have rather then nothing
        callback( std::move( *collected ) );
    }
}

bool AsyncClient::getAllMappings(
        const QUrl& url, const std::function< void( const MappingList& ) >& callback )
{
    auto mappings = std::make_shared< std::vector< MappingInfo > >();
    return getMapping( url, 0,
                       [ this, url, callback, mappings ]( const MappingInfo& newMap )
    {
        fetchMappingsRecursive( this, url, callback, mappings, newMap );
    } );
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS( AsyncClient::Protocol,
    ( AsyncClient::TCP, "tcp" )
    ( AsyncClient::UDP, "udp" )
)

} // namespace nx_upnp
