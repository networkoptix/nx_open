#include "vmax480_resource_searcher.h"
#include "vmax480_resource.h"
#include "utils/common/sleep.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/http/httptypes.h"

#include <QXmlDefaultHandler>


extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);

extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QHostAddress groupAddress(QLatin1String("239.255.255.250"));


//!Partial parser for SSDP descrition xml (UPnP™ Device Architecture 1.1, 2.3)
class UpnpDeviceDescriptionSaxHandler
:
    public QXmlDefaultHandler
{
public:
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
        if( m_currentElementName == QLatin1String("friendlyName") )
            m_friendlyName = ch;
        else if( m_currentElementName == QLatin1String("manufacturer") )
            m_manufacturer = ch;
        else if( m_currentElementName == QLatin1String("modelName") )
            m_modelName = ch;
        else if( m_currentElementName == QLatin1String("serialNumber") )
            m_serialNumber = ch;

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

    QString friendlyName() const { return m_friendlyName; }
    QString manufacturer() const { return m_manufacturer; }
    QString modelName() const { return m_modelName; }
    QString serialNumber() const { return m_serialNumber; }

private:
    QString m_currentElementName;
    QString m_friendlyName;
    QString m_manufacturer;
    QString m_modelName;
    QString m_serialNumber;
};


//====================================================================
QnPlVmax480ResourceSearcher::QnPlVmax480ResourceSearcher()
{

}

QnPlVmax480ResourceSearcher::~QnPlVmax480ResourceSearcher()
{
    foreach(QUdpSocket* sock, m_socketList)
        delete sock;
}

QnPlVmax480ResourceSearcher& QnPlVmax480ResourceSearcher::instance()
{
    static QnPlVmax480ResourceSearcher inst;
    return inst;
}

QUdpSocket* QnPlVmax480ResourceSearcher::sockByName(const QnInterfaceAndAddr& iface)
{
    QMap<QString, QUdpSocket*>::iterator it = m_socketList.find(iface.address.toString());
    if (it == m_socketList.end())
    {
        QUdpSocket* sock = new QUdpSocket();
        if (!bindToInterface(*sock, iface,1900, QUdpSocket::ReuseAddressHint))
        {
            delete sock;
            return 0;
        }

        if (!multicastJoinGroup(*sock, groupAddress, iface.address))
        {
            delete sock;
            return 0;
        }

        m_socketList.insert(iface.address.toString(), sock);

        return sock;
    }
    return it.value();
}

#if 1

QnResourceList QnPlVmax480ResourceSearcher::findResources(void)
{
    
    QnResourceList result;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        QUdpSocket* sock = sockByName(iface);

        QString tmp = iface.address.toString();
        
        
        if (!sock)
            continue;



        //QByteArray requestDatagram;
        //socket.writeDatagram(requestDatagram.data(), requestDatagram.size(), groupAddress, 1900);

        while(sock->hasPendingDatagrams())
        {
            QByteArray reply;
            reply.resize(sock->pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;
            sock->readDatagram(reply.data(), reply.size(),    &sender, &senderPort);

            int index = reply.indexOf("Upnp-DW-VF");

            QString mac;
            QString name;
            int channles = 0;
            if( !(index < 0 || index + 25 > reply.size()) )
            {
                index += 10;

                int index2 = reply.indexOf("-", index);
                if (index2 < 0)
                    continue;

                QByteArray channelsstr = reply.mid(index, index2 - index);

                name = QString(QLatin1String("DW-VF")) + QString(QLatin1String(channelsstr));

                channles = channelsstr.toInt();

                index = index2 + 1;
                index2 = reply.indexOf(":", index);

                if (index2 < 0)
                    continue;

                QByteArray macstr = reply.mid(index, index2 - index);

                mac = QnMacAddress(QString(QLatin1String(macstr))).toString();
            }
            else
            {
                nx_http::HttpRequest foundDeviceReply;
                if( !foundDeviceReply.parse( reply ) )
                    continue;
                nx_http::HttpHeaders::const_iterator locationHeader = foundDeviceReply.headers.find( "LOCATION" );
                if( locationHeader == foundDeviceReply.headers.end() )
                    continue;

                const QUrl descritionUrl( QLatin1String(locationHeader->second) );

                CLSimpleHTTPClient http( descritionUrl.host(), descritionUrl.port(nx_http::DEFAULT_HTTP_PORT), 3000, QAuthenticator() );
                CLHttpStatus status = http.doGET( descritionUrl.path() );
                if (status != CL_HTTP_SUCCESS)
                    continue;
                QByteArray foundDeviceDescription;
                http.readAll( foundDeviceDescription );
                //TODO/IMPL checking Content-Type of received description (MUST be upnp xml description to continue)

                //parsing description xml
                UpnpDeviceDescriptionSaxHandler xmlHandler;
                QXmlSimpleReader xmlReader;
                xmlReader.setContentHandler( &xmlHandler );
                xmlReader.setErrorHandler( &xmlHandler );

                QXmlInputSource input;
                input.setData( foundDeviceDescription );
                if( !xmlReader.parse( &input ) )
                    continue;

                if( xmlHandler.manufacturer().indexOf(QLatin1String("VMAX480")) == -1 )
                    continue;

                mac = xmlHandler.serialNumber();
                //name = xmlHandler.friendlyName();
                //reading channel number from xml
                    //<modelName>VMAX480-8 - 08CH H.264 DVR</modelName>
                const QString& modelName = xmlHandler.modelName();
                const int channelCountEndIndex = modelName.indexOf( QLatin1String("CH") );
                if( channelCountEndIndex == -1 )
                    continue;
                int channelCountStartIndex = modelName.lastIndexOf( QLatin1String(" "), channelCountEndIndex );
                if( channelCountStartIndex == -1 )
                    continue;
                ++channelCountStartIndex;
                if( channelCountStartIndex >= channelCountEndIndex )
                    continue;
                channles = modelName.mid( channelCountStartIndex, channelCountEndIndex-channelCountStartIndex ).toInt();
                name = QLatin1String("DW-VF") + QString::number(channles);  //DW-VF is a registered resource type
            }

            QString host = sender.toString();

            QAuthenticator auth;
            auth.setUser(QLatin1String("admin"));

            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
            if (!rt.isValid())
                continue;

            for (int i = 0; i < channles; ++i)
            {
                QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

                resource->setTypeId(rt);
                resource->setName(name + QString(QLatin1String("-ch%1")).arg(i+1,2));
                (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
                resource->setMAC(mac);

                resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(host).arg(9010).arg(i+1));
                resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(mac).arg(i+1));
                resource->setDiscoveryAddr(iface.address);
                resource->setAuth(auth);

                result << resource;
            }


        }
    }


    return result;
}

#else
QnResourceList QnPlVmax480ResourceSearcher::findResources(void)
{
    QnResourceList result;

    QFile file(QLatin1String("c:/vmax.txt")); // Create a file handle for the file named
    if (!file.exists())
        return result;

    QString line;

    if (!file.open(QIODevice::ReadOnly)) // Open the file
        return result;

    QTextStream stream(&file); // Set the stream to read from myFile

    QnInterfaceAndAddr iface = getAllIPv4Interfaces().first();

    while(1)
    {
        line = stream.readLine(); // this reads a line (QString) from the file

        if (line.trimmed().isEmpty())
            break;

        QStringList params = line.split(QLatin1Char(';'), QString::SkipEmptyParts);

        QString name = params[0];
        QString mac =  params[1];
        QString host =  params[2];
        QString port =  params[3];
        QAuthenticator auth;
        auth.setUser(QLatin1String("admin"));

        for (int i = 0; i < 8; ++i)
        {
            QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );
            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);

            resource->setTypeId(rt);
            resource->setName(name);
            (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
            resource->setMAC(mac);
            resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(host).arg(port).arg(i+1));
            resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(mac).arg(i+1));
            resource->setDiscoveryAddr(iface.address);
            resource->setAuth(auth);

            result << resource;
        }

    }

    return result;
}
#endif

QnResourcePtr QnPlVmax480ResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        //qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnPlVmax480Resource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create test camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QList<QnResourcePtr> QnPlVmax480ResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(doMultichannelCheck)
    QList<QnResourcePtr> result;

    

    CLHttpStatus status;
    QByteArray reply = downloadFile(status, QLatin1String("dvrdevicedesc.xml"),  url.host(), 49152, 1000, auth);

    if (status != CL_HTTP_SUCCESS)
        return result;


    int index = reply.indexOf("Upnp-DW-VF");

    if (index < 0 || index + 25 > reply.size())
        return result;

    index += 10;

    int index2 = reply.indexOf("-", index);
    if (index2 < 0)
        return result;

    QByteArray channelsstr = reply.mid(index, index2 - index);

    QString name = QString(QLatin1String("DW-VF")) + QString(QLatin1String(channelsstr));

    int channles = channelsstr.toInt();

    index = index2 + 1;
    index2 = reply.indexOf("<", index);

    if (index2 < 0)
        return result;


    QByteArray macstr = reply.mid(index, index2 - index);

    QString mac = QnMacAddress(QString(QLatin1String(macstr))).toString();
    QString host = url.host();


    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
        return result;



    for (int i = 0; i < channles; ++i)
    {
        QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

        resource->setTypeId(rt);
        resource->setName(name + QString(QLatin1String("-ch%1")).arg(i+1,2));
        (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
        resource->setMAC(mac);

        resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(host).arg(9010).arg(i+1));
        resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(mac).arg(i+1));
        resource->setAuth(auth);

        result << resource;
    }


    return result;
}

QString QnPlVmax480ResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlVmax480Resource::MANUFACTURE);
}


