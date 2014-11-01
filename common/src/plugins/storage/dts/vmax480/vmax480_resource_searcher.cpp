#ifdef ENABLE_VMAX

#include "vmax480_resource_searcher.h"
#include "vmax480_resource.h"
#include "utils/common/sleep.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/http/httptypes.h"
#include "utils/network/simple_http_client.h"

#include <QtXml/QXmlDefaultHandler>
#include <QtCore/QUrlQuery>

#include "core/resource_management/resource_pool.h"
#include "../../vmaxproxy/src/vmax480_helper.h"


static const int VMAX_API_PORT = 9010;
static const int TCP_TIMEOUT = 3000;
static const QString NAME_PREFIX(QLatin1String("VMAX-"));

//====================================================================
QnPlVmax480ResourceSearcher::QnPlVmax480ResourceSearcher()
{

}

QnPlVmax480ResourceSearcher::~QnPlVmax480ResourceSearcher()
{
}

static QnPlVmax480ResourceSearcher* inst;

void QnPlVmax480ResourceSearcher::initStaticInstance( QnPlVmax480ResourceSearcher* _instance )
{
    inst = _instance;
}

QnPlVmax480ResourceSearcher* QnPlVmax480ResourceSearcher::instance()
{
    return inst;
}


void QnPlVmax480ResourceSearcher::processPacket(const QHostAddress& discoveryAddr,
                                                const QString& host, 
                                                const UpnpDeviceInfo& devInfo,
                                                const QByteArray& /*xmlDevInfo*/,
                                                QnResourceList& result)
{
    QnMacAddress mac(devInfo.serialNumber);
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
    
    int apiPort = VMAX_API_PORT;
    int httpPort = QUrl(devInfo.presentationUrl).port(80);


    QAuthenticator auth;
    auth.setUser(QLatin1String("admin"));

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (rt.isNull())
        return;


    QMap<int, QByteArray> camNames;
    bool camNamesReaded = false;

    QString groupName = QString(QLatin1String("VMAX %1")).arg(host);

    for (int i = 0; i < channles; ++i)
    {
        QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

        resource->setTypeId(rt);

        QString uniqId = QString(QLatin1String("%1_%2")).arg(mac.toString()).arg(i+1);
        bool needHttpData = true;

        QnPlVmax480ResourcePtr existsRes = qnResPool->getResourceByUniqId(uniqId).dynamicCast<QnPlVmax480Resource>();
        if (existsRes && (existsRes->getStatus() == Qn::Online || existsRes->getStatus() == Qn::Recording)) 
        {
            resource->setName(existsRes->getName());
            int existHttpPort = QUrlQuery(QUrl(existsRes->getUrl()).query()).queryItemValue(lit("http_port")).toInt();
            existHttpPort = existHttpPort ? existHttpPort : 80;
            apiPort = QUrl(existsRes->getUrl()).port(VMAX_API_PORT);
            // Prevent constant http pullig. But if http port is changed update api port as well.
            needHttpData = existHttpPort != httpPort;
        }
        
        if (needHttpData)
        {
            if (!camNamesReaded) {
                CLSimpleHTTPClient client(host, httpPort, TCP_TIMEOUT, QAuthenticator());
                if (vmaxAuthenticate(client, auth)) {
                    QByteArray descrPage = readDescriptionPage(client);
                    if (!descrPage.isEmpty()) {
                        camNames = getCamNames(descrPage);
                        apiPort = getApiPort(descrPage);
                    }
                }
                camNamesReaded = true;
            }
            if (camNames.value(i+1).isEmpty())
                resource->setName(name + QString(QLatin1String("-ch%1")).arg(i+1,2));
            else
                resource->setName(NAME_PREFIX + QString::fromUtf8(camNames.value(i+1)));
        }

        (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
        resource->setMAC(mac);

        resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3&http_port=%4")).arg(host).arg(apiPort).arg(i+1).arg(httpPort));
        resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(resource->getMAC().toString()).arg(i+1));
        resource->setAuth(auth);
        resource->setGroupName(groupName);
        QString groupId = QString(QLatin1String("VMAX480_uuid_%1:%2")).arg(host).arg(apiPort);
        resource->setGroupId(groupId);

        result << resource;
    }
}

QnResourcePtr QnPlVmax480ResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
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

    qDebug() << "Create Vmax480 resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;
    //result->deserialize(parameters);

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

QByteArray QnPlVmax480ResourceSearcher::readDescriptionPage(CLSimpleHTTPClient& client)
{
    CLHttpStatus status = client.doGET(QLatin1String("cgi-bin/design/html_template/Camera.cgi"));
    QByteArray answer;
    if (status == CL_HTTP_SUCCESS)
        client.readAll(answer);
    return answer;
}

int QnPlVmax480ResourceSearcher::getApiPort(const QByteArray& answer) const
{
    int result = 0;
    int portIndex = answer.indexOf("param name=\\\"port\\\"");
    if (portIndex > 0) 
    {
        static const QByteArray VALUE_PATTERN("value=\\\"");
        int valIndex = answer.indexOf(VALUE_PATTERN, portIndex);
        if (valIndex > 0) {
            result = extractNum(answer, valIndex + VALUE_PATTERN.length());
        }
    }
    return result ? result : VMAX_API_PORT;
}

QMap<int, QByteArray> QnPlVmax480ResourceSearcher::getCamNames(const QByteArray& answer)
{
    QMap<int, QByteArray> camNames;

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

QList<QnResourcePtr> QnPlVmax480ResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    QList<QnResourcePtr> result;


    int channels = -1;
    int apiPort = VMAX_API_PORT;
    int httpPort = 80;
    QString httpPortStr = QUrlQuery(url.query()).queryItemValue(lit("http_port"));
    int channelNum = QUrlQuery(url.query()).queryItemValue(lit("channel")).toInt();

    if (httpPortStr.isEmpty())
    {
        // first discovery: port used as http port, API port is unknown
        httpPort = url.port(80);
    }
    else {
        // repeat discovery process. port used as API port, http port paced in url parameters. I did it for compatibility with previous version:
        // port in URL parameter must be used as API port

        apiPort = url.port(VMAX_API_PORT);
        httpPort = httpPortStr.toInt();
        if (!httpPort)
            httpPort = 80;
    }


    if (!isSearchAction)
    {
        // it is a fast discovery mode used by resource searcher
        QnPlVmax480ResourcePtr existsRes;
        for (int i = 0; i < VMAX_MAX_CH; ++i) {
            QString uniqId = QString(QLatin1String("VMAX_DVR_%1_%2")).arg(url.host()).arg(i+1);
            existsRes = qnResPool->getResourceByUniqId(uniqId).dynamicCast<QnPlVmax480Resource>();
            if (existsRes)
                break;
        }
        if (existsRes && (existsRes->getStatus() == Qn::Online || existsRes->getStatus() == Qn::Recording)) {
            channels = extractChannelCount(existsRes->getModel().toUtf8()); // avoid real requests
            QUrl url(existsRes->getUrl());
            apiPort = url.port(VMAX_API_PORT);
            int existHttpPort = QUrlQuery(url.query()).queryItemValue(lit("http_port")).toInt();
            if (existHttpPort > 0)
                httpPort = existHttpPort;
        }
    }

    QMap<int, QByteArray> camNames;

    if (channels == -1)
    {
        CLSimpleHTTPClient client(url.host(), httpPort, TCP_TIMEOUT, QAuthenticator());

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
        QByteArray descrPage = readDescriptionPage(client);
        if (!descrPage.isEmpty()) {
            camNames = getCamNames(descrPage);
            apiPort = getApiPort(descrPage);
        }
    }

    QString baseName = QString(QLatin1String("DW-VF")) + QString::number(channels);

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), baseName);
    if (rt.isNull())
        return result;

    QString groupId = QString(QLatin1String("VMAX480_uuid_%1:%2")).arg(url.host()).arg(apiPort);
    QString groupName = QString(QLatin1String("VMAX %1")).arg(url.host());

    int minCh = 0;
    int maxCh = channels;
    if (!isSearchAction)
    {
        minCh = channelNum-1;
        maxCh = channelNum;
    }
    for (int i = minCh; i < maxCh; ++i)
    {
        QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

        resource->setTypeId(rt);
        if (camNames.value(i+1).isEmpty())
            resource->setName(baseName + QString(QLatin1String("-ch%1")).arg(i+1,2));
        else
            resource->setName(NAME_PREFIX + QString::fromUtf8(camNames.value(i+1)));
        (resource.dynamicCast<QnPlVmax480Resource>())->setModel(baseName);
        //resource->setMAC(mac);

        resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3&http_port=%4")).arg(url.host()).arg(apiPort).arg(i+1).arg(httpPort));
        resource->setPhysicalId(QString(QLatin1String("VMAX_DVR_%1_%2")).arg(url.host()).arg(i+1));
        resource->setAuth(auth);
        resource->setGroupId(groupId);
        resource->setGroupName(groupName);

        result << resource;

    }

    return result;
}

QString QnPlVmax480ResourceSearcher::manufacture() const
{
    return QnPlVmax480Resource::MANUFACTURE;
}

#endif // #ifdef ENABLE_VMAX
