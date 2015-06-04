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

bool UpnpAsyncClient::doUpnp( const QUrl& url, const Message& message, const Callback& callback )
{
    const auto service = toUpnpUrn( message.service, lit("service") );
    const auto action = lit( "\"%1#%2\"" ).arg( service ).arg( message.action );
    addAdditionalHeader( "SOAPAction", action.toUtf8() );

    QStringList params;
    for( const auto& p : message.params )
        params.push_back( lit( "<%1>%2</%1>" ).arg( p.first ).arg( p.second ) );

    const auto request = SOAP_REQUEST.arg( message.action ).arg( service )
                                     .arg( params.join( lit("") ) );

    const auto complete = [this, callback]( const nx_http::AsyncHttpClientPtr& ptr )
    {
        if( hasRequestSuccesed() )
        {
            UpnpMessageHandler xmlHandler;
            QXmlSimpleReader xmlReader;
            xmlReader.setContentHandler( &xmlHandler );
            xmlReader.setErrorHandler( &xmlHandler );

            const auto msg = fetchMessageBodyBuffer();
            QXmlInputSource input;
            input.setData( msg );
            if( xmlReader.parse( &input ) )
            {
                callback( xmlHandler.message() );
                return;
            }
        }

        callback( Message() ); // error
    };

    connect( this, &nx_http::AsyncHttpClient::done,
             this, complete, Qt::DirectConnection );

    return doPost( url, "text/xml", request.toUtf8() );
}
