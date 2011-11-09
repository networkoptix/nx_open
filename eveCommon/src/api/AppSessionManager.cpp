#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>

#include "QStdIStream.h"

#include "AppSessionManager.h"

AppSessionManager::AppSessionManager(const QHostAddress& host, int port, const QAuthenticator& auth)
    : SessionManager(host, port, auth)
{
}

int AppSessionManager::addServer(const ::xsd::api::servers::Server& server, QnApiServerResponsePtr& serversPtr)
{
    std::ostringstream os;

    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);
    ::xsd::api::servers::servers(os, servers);

    QByteArray reply;
    CLHttpStatus status = addObject("server", os.str().c_str(), reply);

    if (status == CL_HTTP_SUCCESS)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            serversPtr = QnApiServerResponsePtr(xsd::api::servers::servers (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            qDebug(e.what());
        }
    }

    return 1;
}

int AppSessionManager::addCamera(const ::xsd::api::cameras::Camera& camera, QnApiCameraResponsePtr& camerasPtr)
{
    std::ostringstream os;

    ::xsd::api::cameras::Cameras cameras;
    cameras.camera().push_back(camera);
    ::xsd::api::cameras::cameras(os, cameras);

    QByteArray reply;
    CLHttpStatus status = addObject("camera", os.str().c_str(), reply);

    if (status == CL_HTTP_SUCCESS)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            camerasPtr = QnApiCameraResponsePtr(xsd::api::cameras::cameras (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            qDebug(e.what());
        }
    }

    return 1;
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

CLHttpStatus AppSessionManager::addObject(const QString& objectName, const QByteArray& body, QByteArray& reply)
{
    CLHttpStatus status = m_client.doPOST(QString("api/%1/").arg(objectName), body);

    if (status == CL_HTTP_SUCCESS || status == CL_HTTP_BAD_REQUEST)
        m_client.readAll(reply);

    return status;
}

int AppSessionManager::getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes)
{
    QByteArray reply;

    if(sendGetRequest("resourceType", reply) == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            resourceTypes = QnApiResourceTypeResponsePtr(xsd::api::resourceTypes::resourceTypes (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            qDebug(e.what());
        }
    }

    return 1;
}

int AppSessionManager::getResources(QnApiResourceResponsePtr& resources)
{
    QByteArray reply;

    if (sendGetRequest("resourceEx", reply) == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            resources = QnApiResourceResponsePtr(xsd::api::resourcesEx::resourcesEx (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            qDebug(e.what());
        }
    }

    return 1;
}
