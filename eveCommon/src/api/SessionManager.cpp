#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>

#include "QStdIStream.h"

#include "SessionManager.h"

SessionManager::SessionManager(const QHostAddress& host, const QAuthenticator& auth)
    : m_client(host, 8000, 10000, auth)
{
}

int SessionManager::addServer(const ::xsd::api::servers::Server& server, QnApiServerResponsePtr& serversPtr)
{
    std::ostringstream os;

    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);
    ::xsd::api::servers::servers(os, servers);

    QByteArray reply;
    int status = addObject("server", os.str().c_str(), reply);

    try
    {
        QTextStream stream(reply);
        QStdIStream is(stream.device());

        serversPtr = QnApiServerResponsePtr(xsd::api::servers::servers (is, xml_schema::flags::dont_validate).release());
    } catch (const xml_schema::exception& e)
    {
        qDebug(e.what());
    }

    return status;
}

int SessionManager::addCamera(const ::xsd::api::cameras::Camera& camera, QnApiCameraResponsePtr& camerasPtr)
{
    std::ostringstream os;

    ::xsd::api::cameras::Cameras cameras;
    cameras.camera().push_back(camera);
    ::xsd::api::cameras::cameras(os, cameras);

    QByteArray reply;
    int status = addObject("camera", os.str().c_str(), reply);

    try
    {
        QTextStream stream(reply);
        QStdIStream is(stream.device());

        camerasPtr = QnApiCameraResponsePtr(xsd::api::cameras::cameras (is, xml_schema::flags::dont_validate).release());
    } catch (const xml_schema::exception& e)
    {
        qDebug(e.what());
    }

    return status;
}

#if 0
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

        if (objectName == "resourceEx")
        {
            typedef xsd::api::resourcesEx::Resources T;
            QSharedPointer<T> result;


            emit resourcesReceived((RequestId)reply, result);
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
#endif

int SessionManager::getObjectList(QString objectName, QByteArray& reply)
{
    m_client.doGET(QString("api/%1/").arg(objectName));

//    qDebug() << "Connected: " << m_client.isOpened();

    m_client.readAll(reply);

//    qDebug() << "Reply: " << reply.data();

    return 0;
}

int SessionManager::addObject(const QString& objectName, const QByteArray& body, QByteArray& response)
{
    m_client.doGET(QString("/api/%1/").arg(objectName));

    return 0;
}

int SessionManager::getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes)
{
    QByteArray reply;

    int status = getObjectList("resourceType", reply);

    try
    {
        QTextStream stream(reply);
        QStdIStream is(stream.device());

        resourceTypes = QnApiResourceTypeResponsePtr(xsd::api::resourceTypes::resourceTypes (is, xml_schema::flags::dont_validate).release());
    } catch (const xml_schema::exception& e)
    {
        qDebug(e.what());
    }

    return status;
}

int SessionManager::getResources(QnApiResourceResponsePtr& resources)
{
    QByteArray reply;

    int status = getObjectList("resourceEx", reply);

    try
    {
        QTextStream stream(reply);
        QStdIStream is(stream.device());

        resources = QnApiResourceResponsePtr(xsd::api::resourcesEx::resourcesEx (is, xml_schema::flags::dont_validate).release());
    } catch (const xml_schema::exception& e)
    {
        qDebug(e.what());
    }

    return status;
}
