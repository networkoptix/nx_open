#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>

#include "SessionManager.h"

SessionManager::SessionManager(const QString& host, const QString& login, const QString& password)
    : m_manager(this),
      m_host(host),
      m_port(8000),
      m_login(login),
      m_password(password),
      m_needEvents(false)

{
    QObject::connect(&m_manager, SIGNAL(finished(QNetworkReply *)),
                              SLOT(slotRequestFinished(QNetworkReply *)));
}

void SessionManager::openEventChannel()
{
    m_needEvents = true;

    QUrl url;
    url.setScheme("http");
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath("/api/event/");

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Authorization", "Basic " +
                         QByteArray(QString("%1:%2").arg(m_login).arg(m_password).toAscii()).toBase64());

    QNetworkReply *reply = m_manager.get(request);
    m_requestObjectMap[reply] = "event";
}

void SessionManager::closeEventChannel()
{
    m_needEvents = false;
}

bool hasChildElements(QDomNode node)
{
    return !node.firstChildElement().isNull();
}

template<class T>
void readObjectList(QDomNode node, QList<T*>* objects)
{
    QString nodeName = node.nodeName();

    node = node.firstChild();
    while (!node.isNull())
    {
        T *object = new T();

        QDomNode attrNode = node.firstChild();
        while (!attrNode.isNull())
        {
            //qDebug() << "Property Name: " << attrNode.nodeName().toAscii();
            //qDebug() << "Property Value: " << attrNode.toElement().text();

            QByteArray propertyName = attrNode.nodeName().toAscii();

            if (hasChildElements(attrNode))
            {
                QList<T*>* children = new QList<T*>();

                readObjectList<T>(attrNode, children);
                object->setProperty(propertyName, qVariantFromValue(children));
            } else
            {
                object->setProperty(propertyName, attrNode.toElement().text());
            }

            attrNode = attrNode.nextSibling();
        }

        objects->append(object);
        node = node.nextSibling();
    }
}

template<class T>
void readObjectList(QNetworkReply *reply, QList<T*>* objects)
{
    QDomDocument doc;
    QXmlInputSource is;
    is.setData(reply->readAll());
    if (!doc.setContent(is.data()))
    {
        qDebug("Failed to setContent");
        qDebug() << is.data();

        return;
    }

    qDebug() << "Raw data: " << is.data();

    QDomNode node = doc.documentElement();
    QString nodeName = node.nodeName();
    readObjectList(node, objects);
}

void SessionManager::slotRequestFinished(QNetworkReply *reply)
{
    qDebug("slotRequestFinished");

    if (reply->error() > 0) {
        qDebug() << reply->errorString();
    }
    else {
        QString objectName;
        if (m_requestObjectMap.contains(reply))
        {
            objectName = m_requestObjectMap[reply];
            m_requestObjectMap.remove(reply);
        }

        if (objectName == "event")
        {
            if (m_needEvents)
                openEventChannel();

            QList<Event*> *events = new QList<Event*>();
            readObjectList<Event>(reply, events);

            emit eventsReceived(events);
        } else if (objectName == "camera")
        {
            QList<Camera*> *cameras = new QList<Camera*>();
            readObjectList<Camera>(reply, cameras);

            emit camerasReceived((RequestId)reply, cameras);
        } else if (objectName == "resourceType")
        {
            QList<ResourceType*> *resourceTypes = new QList<ResourceType*>();
            readObjectList<ResourceType>(reply, resourceTypes);

            emit resourceTypesReceived((RequestId)reply, resourceTypes);
        } else if (objectName == "server")
        {
            QList<Server*> *servers = new QList<Server*>();
            readObjectList<Server>(reply, servers);

            emit serversReceived((RequestId)reply, servers);
        } else if (objectName == "layout")
        {
            QList<Layout*> *layouts = new QList<Layout*>();
            readObjectList<Layout>(reply, layouts);

            emit layoutsReceived((RequestId)reply, layouts);
        } else if (objectName == "resource")
        {
            QList<Resource*> *resources = new QList<Resource*>();
            readObjectList<Resource>(reply, resources);

            emit resourcesReceived((RequestId)reply, resources);
        }
    }
}

RequestId SessionManager::getObjectList(QString objectName, bool tree)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(m_host);
    url.setPort(m_port);
    if (tree)
    {
        url.setPath(QString("/api/%1/tree/").arg(objectName));
        url.addQueryItem("id", "3");
    }
    else
    {
        url.setPath(QString("/api/%1/").arg(objectName));
    }


    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Authorization", "Basic " +
                         QByteArray(QString("%1:%2").arg(m_login).arg(m_password).toAscii()).toBase64());

    QNetworkReply *reply = m_manager.get(request);
    m_requestObjectMap[reply] = objectName;

    return (RequestId) reply;
}

RequestId SessionManager::addObject(const Object& object, const QString& additionalArgs)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(QString("/api/%1/").arg(object.objectName()));

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Authorization", "Basic " +
                         QByteArray(QString("%1:%2").arg(m_login).arg(m_password).toAscii()).toBase64());

    QByteArray body = object.serialize();
    if (!additionalArgs.isEmpty())
        body += "&" + additionalArgs.toAscii();

    QNetworkReply *reply = m_manager.post(request, body);
    m_requestObjectMap[reply] = object.objectName();

    return (RequestId) reply;
}

RequestId SessionManager::getResourceTypes()
{
    return getObjectList("resourceType", false);
}

RequestId SessionManager::getCameras()
{
    return getObjectList("camera", false);
}

RequestId SessionManager::getLayouts()
{
    return getObjectList("layout", false);
}

RequestId SessionManager::getServers()
{
    return getObjectList("server", false);
}

RequestId SessionManager::getResources(bool tree)
{
    return getObjectList("resource", tree);
}

RequestId SessionManager::addServer(const Server& server)
{
    return addObject(server);
}

RequestId SessionManager::addLayout(const Layout& layout)
{
    return addObject(layout);
}

RequestId SessionManager::addCamera(const Camera& camera, const QString& serverId)
{
    QString additionalArgs = QString("server_id=%1").arg(serverId);

    return addObject(camera, additionalArgs);
}
