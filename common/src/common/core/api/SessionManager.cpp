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

template<class T>
void readObjectList(QNetworkReply *reply, QList<T*>& objects)
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

    QDomNode node = doc.documentElement().firstChild();
    while (!node.isNull())
    {
        T *object = new T();

        QDomNode attrNode = node.firstChild();
        while (!attrNode.isNull())
        {
            //qDebug() << "Property Name: " << attrNode.nodeName().toAscii();
            //qDebug() << "Property Value: " << attrNode.toElement().text();

            object->setProperty(attrNode.nodeName().toAscii(), attrNode.toElement().text());
            attrNode = attrNode.nextSibling();
        }

        objects.append(object);
        node = node.nextSibling();
    }
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
            readObjectList<Event>(reply, *events);

            emit eventsReceived(events);
        } else if (objectName == "camera")
        {
            QList<Camera*> *cameras = new QList<Camera*>();
            readObjectList<Camera>(reply, *cameras);

            emit camerasReceived((int)reply, cameras);
        } else if (objectName == "resourceType")
        {
            QList<ResourceType*> *resourceTypes = new QList<ResourceType*>();
            readObjectList<ResourceType>(reply, *resourceTypes);

            emit resourceTypesReceived((int)reply, resourceTypes);
        } else if (objectName == "server")
        {
            QList<Server*> *servers = new QList<Server*>();
            readObjectList<Server>(reply, *servers);

            emit serversReceived((int)reply, servers);
        }
    }
}

int SessionManager::getObjectList(QString objectName)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(QString("/api/%1/").arg(objectName));

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Authorization", "Basic " +
                         QByteArray(QString("%1:%2").arg(m_login).arg(m_password).toAscii()).toBase64());

    QNetworkReply *reply = m_manager.get(request);
    m_requestObjectMap[reply] = objectName;

    return (int) reply;
}

int SessionManager::addObject(const Object& object, const QString& additionalArgs)
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

    return (int) reply;
}

int SessionManager::getResourceTypes()
{
    return getObjectList("resourceType");
}

int SessionManager::getCameras()
{
    return getObjectList("camera");
}

int SessionManager::addServer(const Server& server)
{
    return addObject(server);
}

int SessionManager::addCamera(const Camera& camera, const QString& serverId)
{
    QString additionalArgs = QString("server_id=%1").arg(serverId);

    return addObject(camera, additionalArgs);
}
