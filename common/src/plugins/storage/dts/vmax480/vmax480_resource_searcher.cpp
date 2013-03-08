#include "vmax480_resource_searcher.h"
#include "vmax480_resource.h"
#include "utils/common/sleep.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/http/httptypes.h"
#include "utils/network/simple_http_client.h"

#include <QXmlDefaultHandler>
#include "core/resource_managment/resource_pool.h"
#include "../../vmaxproxy/src/vmax480_helper.h"


static const int API_PORT = 9010;
static const int TCP_TIMEOUT = 3000;
static const QString NAME_PREFIX(QLatin1String("VMAX-"));

//====================================================================
QnPlVmax480ResourceSearcher::QnPlVmax480ResourceSearcher()
{

}

QnPlVmax480ResourceSearcher::~QnPlVmax480ResourceSearcher()
{
}

QnPlVmax480ResourceSearcher& QnPlVmax480ResourceSearcher::instance()
{
    static QnPlVmax480ResourceSearcher inst;
    return inst;
}


void QnPlVmax480ResourceSearcher::processPacket(const QHostAddress& discoveryAddr,
                                                const QString& host, 
                                                const BonjurDeviceInfo& devInfo,
                                                QnResourceList& result)
{
    QString mac = devInfo.serialNumber;
    const int channelCountEndIndex = devInfo.modelName.indexOf( QLatin1String("CH") );
    if( channelCountEndIndex == -1 )
        return;
    int channelCountStartIndex = devInfo.modelName.lastIndexOf( QLatin1String(" "), channelCountEndIndex );
    if( channelCountStartIndex == -1 )
        return;
    ++channelCountStartIndex;
    if( channelCountStartIndex >= channelCountEndIndex )
        return;
    int channles = devInfo.modelName.mid( channelCountStartIndex, channelCountEndIndex-channelCountStartIndex ).toInt();
    QString name = QLatin1String("DW-VF") + QString::number(channles);  //DW-VF is a registered resource type


    QAuthenticator auth;
    auth.setUser(QLatin1String("admin"));

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
        return;


    QMap<int, QByteArray> camNames;
    bool camNamesReaded = false;

    QString groupId = QString(QLatin1String("VMAX480_uuid_%1:%2")).arg(host).arg(API_PORT);
    QString groupName = QString(QLatin1String("VMAX %1")).arg(host);

    for (int i = 0; i < channles; ++i)
    {
        QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

        resource->setTypeId(rt);

        QString uniqId = QString(QLatin1String("%1_%2")).arg(mac).arg(i+1);
        QnPlVmax480ResourcePtr existsRes = qnResPool->getResourceByUniqId(uniqId).dynamicCast<QnPlVmax480Resource>();
        if (existsRes)
            resource->setName(existsRes->getName());
        else {
            if (!camNamesReaded) {
                CLSimpleHTTPClient client(host, 80, TCP_TIMEOUT, QAuthenticator());
                if (vmaxAuthenticate(client, auth))
                    camNames = getCamNames(client);
                camNamesReaded = true;
            }
            if (camNames.value(i+1).isEmpty())
                resource->setName(name + QString(QLatin1String("-ch%1")).arg(i+1,2));
            else
                resource->setName(NAME_PREFIX + QString::fromUtf8(camNames.value(i+1)));
        }

        (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
        resource->setMAC(mac);

        resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(host).arg(API_PORT).arg(i+1));
        resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(resource->getMAC().toString()).arg(i+1));
        resource->setDiscoveryAddr(discoveryAddr);
        resource->setAuth(auth);
        resource->setGroupName(groupName);
        resource->setGroupId(groupId);

        result << resource;
    }
}

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

int extractNum(const QByteArray data, int pos)
{
    int result = 0;
    for (;pos < data.size() && data.at(pos) >= '0' && data.at(pos) <= '9'; ++pos)
    {
        result *= 10;
        result += data.at(pos) - '0';
    }
    return result;
}

QByteArray extractCamName(const QByteArray& request, int pos)
{
    QByteArray rez;
    if (pos >= request.size() || request.at(pos) != '+')
        return rez;
    ++pos;
    for (;pos < request.size() && request.at(pos) != '+' && request.at(pos) != '"'; ++pos)
    {
        rez += request.at(pos);
    }
    return rez;
}

QMap<int, QByteArray> QnPlVmax480ResourceSearcher::getCamNames(CLSimpleHTTPClient& client)
{
    QMap<int, QByteArray> camNames;

    CLHttpStatus status = client.doGET(QLatin1String("cgi-bin/design/html_template/Camera.cgi"));
    if (status == CL_HTTP_SUCCESS)
    {
        QByteArray answer;
        client.readAll(answer);
        QByteArray pattern1("\"hid_camera_info_");
        QByteArray pattern2("value=\"");
        int pos = answer.indexOf(pattern1);
        while (pos >= 0) {
            int chNum = extractNum(answer, pos + pattern1.length());
            if (chNum > 0) 
            {
                int valuePos = answer.indexOf(pattern2, pos);
                if (valuePos > 0) {
                    QByteArray value = extractCamName(answer, valuePos + pattern2.length());
                    if (!value.isEmpty())
                        camNames[chNum] = value;
                }
            }
            pos = answer.indexOf(pattern1, pos+1);
        }
    }

    return camNames;
}

bool QnPlVmax480ResourceSearcher::vmaxAuthenticate(CLSimpleHTTPClient& client, const QAuthenticator& auth)
{
    QString body(QLatin1String("login_txt_id=%1&login_txt_pw=%2"));
    body = body.arg(auth.user()).arg(auth.password());
    CLHttpStatus status = client.doPOST(QLatin1String("cgi-bin/design/html_template/Login.cgi"), body);

    if (status != CL_HTTP_SUCCESS)
        return false;

    QByteArray answer;
    client.readAll(answer);

    QByteArray sessionId = client.getHeaderValue("Set-Cookie");

    client.close();

    if (sessionId.isEmpty())
        return false;

    client.addHeader("Cookie", sessionId);
    return true;
}

int extractChannelCount(const QByteArray& model)
{
    QByteArray num;
    for (int i = model.size()-1; i >= 0; --i)
    {
        if (model.at(i) >= '0' && model.at(i) <= '9')
            num = model.at(i) + num;
    }
    return num.toInt();
}

QList<QnResourcePtr> QnPlVmax480ResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    QList<QnResourcePtr> result;

    int channels = -1;

    if (!doMultichannelCheck)
    {
        // it is a fast discovery mode used by resource searcher
        QnPlVmax480ResourcePtr existsRes;
        for (int i = 0; i < VMAX_MAX_CH; ++i) {
            QString uniqId = QString(QLatin1String("VMAX_DVR_%1_%2")).arg(url.host()).arg(i+1);
            existsRes = qnResPool->getResourceByUniqId(uniqId).dynamicCast<QnPlVmax480Resource>();
            if (existsRes)
                break;
        }
        if (existsRes && (existsRes->getStatus() == QnResource::Online || existsRes->getStatus() == QnResource::Recording))
            channels = extractChannelCount(existsRes->getModel().toUtf8()); // avoid real requests
    }

    QMap<int, QByteArray> camNames;

    if (channels == -1)
    {
        CLSimpleHTTPClient client(url.host(), 80, TCP_TIMEOUT, QAuthenticator());

        if (!vmaxAuthenticate(client, auth))
            return result;

        CLHttpStatus status = client.doGET(QLatin1String("cgi-bin/design/html_template/webviewer.cgi"));
        if (status != CL_HTTP_SUCCESS)
            return result;

        QByteArray answer;
        client.readAll(answer);
        client.close();

        int pos = answer.indexOf("\"btn_ch");
        while (pos >= 0) {
            channels = qMax(channels, extractNum(answer, pos+7));
            pos = answer.indexOf("\"btn_ch", pos+1);
        }

        client.close();

        camNames = getCamNames(client);
    }

    QString baseName = QString(QLatin1String("DW-VF")) + QString::number(channels);

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), baseName);
    if (!rt.isValid())
        return result;

    QString groupId = QString(QLatin1String("VMAX480_uuid_%1:%2")).arg(url.host()).arg(API_PORT);
    QString groupName = QString(QLatin1String("VMAX %1")).arg(url.host());

    for (int i = 0; i < channels; ++i)
    {
        QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

        resource->setTypeId(rt);
        if (camNames.value(i+1).isEmpty())
            resource->setName(baseName + QString(QLatin1String("-ch%1")).arg(i+1,2));
        else
            resource->setName(NAME_PREFIX + QString::fromUtf8(camNames.value(i+1)));
        (resource.dynamicCast<QnPlVmax480Resource>())->setModel(baseName);
        //resource->setMAC(mac);

        resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(url.host()).arg(API_PORT).arg(i+1));
        resource->setPhysicalId(QString(QLatin1String("VMAX_DVR_%1_%2")).arg(url.host()).arg(i+1));
        resource->setAuth(auth);
        resource->setGroupId(groupId);
        resource->setGroupName(groupName);

        result << resource;

    }

    return result;
}

/*
QList<QnResourcePtr> QnPlVmax480ResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
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

        resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(host).arg(API_PORT).arg(i+1));
        resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(mac).arg(i+1));
        resource->setAuth(auth);

        result << resource;
    }


    return result;
}
*/

QString QnPlVmax480ResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlVmax480Resource::MANUFACTURE);
}


