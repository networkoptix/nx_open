#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>

#include "QStdIStream.h"

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

RequestId SessionManager::addServer(const ::xsd::api::servers::Server& server)
{
    std::ostringstream os;

    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);
    ::xsd::api::servers::servers(os, servers);

    return addObject("server", os.str().c_str());
}

RequestId SessionManager::addCamera(const ::xsd::api::cameras::Camera& camera)
{
    std::ostringstream os;

    ::xsd::api::cameras::Cameras cameras;
    cameras.camera().push_back(camera);
    ::xsd::api::cameras::cameras(os, cameras);

    return addObject("camera", os.str().c_str());
}

#if 0
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

#endif

void SessionManager::slotRequestFinished(QNetworkReply *reply)
{
    qDebug("slotRequestFinished");

    if (reply->error() > 0)
    {
        QString message;
        QTextStream stream(&message);

        stream << reply->errorString() << reply->readAll();
        emit error((RequestId)reply, message);
    }
    else
    {
        QString objectName;
        if (m_requestObjectMap.contains(reply))
        {
            objectName = m_requestObjectMap[reply];
            m_requestObjectMap.remove(reply);
        }

        QStdIStream is(reply);

        if (objectName == "resourceType")
        {
            typedef xsd::api::resourceTypes::ResourceTypes T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::resourceTypes::resourceTypes (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit resourceTypesReceived((RequestId)reply, result);
        }

        if (objectName == "camera")
        {
            typedef xsd::api::cameras::Cameras T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::cameras::cameras (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit camerasReceived((RequestId)reply, result);
        }

        if (objectName == "resourceEx")
        {
            typedef xsd::api::resourcesEx::Resources T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::resourcesEx::resourcesEx (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit resourcesReceived((RequestId)reply, result);
        }

        if (objectName == "server")
        {
            typedef xsd::api::servers::Servers T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::servers::servers (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit serversReceived((RequestId)reply, result);
        }

        if (objectName == "layout")
        {
            typedef xsd::api::layouts::Layouts T;
            QSharedPointer<T> result;

            try
            {
                result = QSharedPointer<T>(xsd::api::layouts::layouts (is, xml_schema::flags::dont_validate).release());
            } catch (const xml_schema::exception& e)
            {
                qDebug(e.what());
            }

            emit layoutsReceived((RequestId)reply, result);
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

RequestId SessionManager::addObject(const QString& objectName, const QByteArray& body)
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

    QNetworkReply *reply = m_manager.post(request, body);
    m_requestObjectMap[reply] = objectName;

    return (RequestId) reply;
}

RequestId SessionManager::getResourceTypes()
{
    return getObjectList("resourceType", false);
}

RequestId SessionManager::getResources()
{
    return getObjectList("resourceEx", false);
}
