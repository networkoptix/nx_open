/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "discovery_manager.h"

#include <cstdlib>
#include <cstddef>
#include <cstring>

#include <QtNetwork/QAuthenticator>
#include <QtCore/QLatin1String>
#include <QtXml/QXmlDefaultHandler>

#include "cam_params.h"
#include "camera_manager.h"
#include "camera_plugin.h"
#include "sync_http_client.h"


CameraDiscoveryManager::CameraDiscoveryManager()
:
    m_refManager( CameraPlugin::instance()->refManager() )
{
}

CameraDiscoveryManager::~CameraDiscoveryManager()
{
}

void* CameraDiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

int CameraDiscoveryManager::addRef() const
{
    return m_refManager.addRef();
}

int CameraDiscoveryManager::releaseRef() const
{
    return m_refManager.releaseRef();
}

static const char* VENDOR_NAME = "evidence";

void CameraDiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int CameraDiscoveryManager::findCameras( nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/ )
{
    //supporting only MDNS-discovery
    return 0;
}

int CameraDiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password )
{
    QString host;
    int port = DEFAULT_CAMERA_API_PORT;

    const char* sepPos = strchr( address, ':' );
    if( sepPos )
    {
        host = QString::fromLatin1( address, sepPos-address );
        port = atoi( sepPos+1 );
    }
    else
    {
        //no port specified
        host = QString::fromLatin1( address );
    }

    QAuthenticator credentials;
    const char* loginToUse = login ? login : AXIS_DEFAULT_LOGIN;
    const char* passwordToUse = password ? password : AXIS_DEFAULT_PASSWORD;
    credentials.setUser( QString::fromUtf8(loginToUse) );
    credentials.setPassword( QString::fromUtf8(passwordToUse) );

    SyncHttpClient http(
        CameraPlugin::instance()->networkAccessManager(),
        host,
        port,
        credentials );
    if( http.get( QLatin1String("axis-cgi/param.cgi?action=list&group=root.Properties.Firmware.Version,root.Network.eth0.MACAddress,root.Network.Bonjour.FriendlyName") ) != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;
    if( http.statusCode() != SyncHttpClient::HTTP_OK )
        return http.statusCode() == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;
    const QByteArray& responseBody = http.readWholeMessageBody();
    if( responseBody.isEmpty() )
        return 0;
    const QList<QByteArray>& lines = responseBody.split( '\n' );

    QByteArray modelName;
    QByteArray mac;
    QByteArray firmware;
    foreach( QByteArray line, lines )
    {
        //line has format param=value
        int paramValueSepPos = line.indexOf('=');
        if( paramValueSepPos == -1 )
            continue;   //unknown param format
        const QByteArray& paramName = line.mid(0, paramValueSepPos).trimmed();
        const QByteArray& paramValue = line.mid(paramValueSepPos+1).trimmed();

        if( paramName == "root.Network.Bonjour.FriendlyName" )  //has format: <product name> - <serialnumber>
            modelName = paramValue.mid(0, paramValue.indexOf('-')).trimmed();
        else if( paramName == "root.Network.eth0.MACAddress" )
            mac = paramValue;
        else if( paramName == "root.Properties.Firmware.Version" )
            firmware = paramValue;
    }

    if( modelName.isEmpty() || mac.isEmpty() )
        return 0;

    memset( cameras, 0, sizeof(*cameras) );
    strncpy( cameras->uid, mac.constData(), sizeof(cameras->uid)-1 );
    strncpy( cameras->modelName, modelName.constData(), sizeof(cameras->modelName)-1 );
    strncpy( cameras->firmware, firmware.constData(), sizeof(cameras->firmware)-1 );
    const QByteArray& hostUtf8 = host.toUtf8();
    strncpy( cameras->url, hostUtf8.constData(), sizeof(cameras->url)-1 );
    strcpy( cameras->defaultLogin, loginToUse );
    strcpy( cameras->defaultPassword, passwordToUse );

    return 1;
}

int CameraDiscoveryManager::fromMDNSData(
    const char* discoveredAddress,
    const unsigned char* mdnsResponsePacket,
    int mdnsResponsePacketSize,
    nxcip::CameraInfo* cameraInfo )
{
#if 0
    const QByteArray& msdnResponse = QByteArray::fromRawData( (const char*)mdnsResponsePacket, mdnsResponsePacketSize );

    //parsing mdns response
    QByteArray smac;
    QByteArray name;

    int iqpos = msdnResponse.indexOf("AXIS");

    if (iqpos<0)
        return 0;

    int macpos = msdnResponse.indexOf("00", iqpos);
    if (macpos < 0)
        return 0;

    for (int i = iqpos; i < macpos; i++)
    {
        name += (char)msdnResponse[i];
    }

    name.replace(' ', QByteArray()); // remove spaces
    name.replace('-', QByteArray()); // remove spaces
    name.replace('\t', QByteArray()); // remove tabs

    if (macpos+12 > msdnResponse.size())
        return 0;

    //macpos++; // -

    while(msdnResponse.at(macpos)==' ')
        ++macpos;

    if (macpos+12 > msdnResponse.size())
        return 0;

    for (int i = 0; i < 12; i++)
    {
        if (i > 0 && i % 2 == 0)
            smac += '-';

        smac += (char)msdnResponse[macpos + i];
    }

    smac = smac.toUpper();

    memset( cameraInfo, 0, sizeof(*cameraInfo) );
    strncpy( cameraInfo->uid, smac.constData(), sizeof(cameraInfo->uid)-1 );
    strncpy( cameraInfo->modelName, name.constData(), sizeof(cameraInfo->modelName)-1 );
    strncpy( cameraInfo->defaultLogin, AXIS_DEFAULT_LOGIN, sizeof(cameraInfo->defaultLogin)-1 );
    strncpy( cameraInfo->defaultPassword, AXIS_DEFAULT_PASSWORD, sizeof(cameraInfo->defaultPassword)-1 );
    strncpy( cameraInfo->url, discoveredAddress, sizeof(cameraInfo->url)-1 );

    return 1;
#else
    return 0;
#endif
}

//!Partial parser for SSDP descrition xml (UPnP Device Architecture 1.1, 2.3)
class UpnpDeviceDescriptionSaxHandler
:
    public QXmlDefaultHandler
{
public:
    UpnpDeviceDescriptionSaxHandler( nxcip::CameraInfo* cameraInfo )
    :
        m_cameraInfo( cameraInfo ),
        m_isEvidence( false )
    {
    }

    virtual bool startDocument()
    {
        return true;
    }

    virtual bool startElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& /*atts*/ )
    {
        m_currentElementName = qName;
        return true;
    }

    virtual bool characters( const QString& ch )
    {
        if( m_currentElementName == QLatin1String("modelName") )
            strcpy( m_cameraInfo->modelName, ch.toLatin1().constData() );
        else if( m_currentElementName == QLatin1String("UDN") )
            strcpy( m_cameraInfo->uid, ch.toLatin1().constData() );
        else if( m_currentElementName == QLatin1String("presentationURL") )
            strcpy( m_cameraInfo->url, ch.toLatin1().constData() );
        else if( m_currentElementName == QLatin1String("manufacturerURL") )
            m_isEvidence = ch.contains(QString::fromLatin1("www.e-vidence.ru"));

        return true;
    }

    virtual bool endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/ )
    {
        m_currentElementName.clear();
        return true;
    }

    virtual bool endDocument()
    {
        return true;
    }

    bool isEvidence()
    {
        return m_isEvidence;
    }

private:
    nxcip::CameraInfo* m_cameraInfo;
    bool m_isEvidence;
    QString m_currentElementName;
};


int CameraDiscoveryManager::fromUpnpData( const char* upnpXMLData, int upnpXMLDataSize, nxcip::CameraInfo* cameraInfo )
{
    UpnpDeviceDescriptionSaxHandler xmlHandler( cameraInfo );

    QXmlSimpleReader xmlReader;
    xmlReader.setContentHandler( &xmlHandler );
    xmlReader.setErrorHandler( &xmlHandler );

    QXmlInputSource input;
    input.setData( QByteArray::fromRawData( upnpXMLData, upnpXMLDataSize ) );
    if( !xmlReader.parse( &input ) )
        return 0;

    strcpy( cameraInfo->defaultLogin, "Admin" );
    strcpy( cameraInfo->defaultPassword, "1234" );

    return xmlHandler.isEvidence() ? 1 : 0;
}

nxcip::BaseCameraManager* CameraDiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
{
    CameraManager* obj = new CameraManager( info );
    return obj;
}

int CameraDiscoveryManager::getReservedModelList( char** modelList, int* count )
{
#if 1
    if( !modelList )
    {
        *count = 1;
        return nxcip::NX_NO_ERROR;
    }

    if( *count >= 1 )
    {
        *count = 1;
        strcpy( modelList[0], "*Apix*" );
    }
    else
    {
        *count = 1;
        return nxcip::NX_MORE_DATA;
    }
#else
    *count = 0;
#endif

    return nxcip::NX_NO_ERROR;
}
