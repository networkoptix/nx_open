#ifndef _APP_SESSION_MANAGER_H
#define _APP_SESSION_MANAGER_H

#include <QtCore/QSharedPointer>

#include "api/Types.h"
#include "utils/network/simple_http_client.h"
#include "utils/common/qnid.h"

#include "QStdIStream.h"

#include "SessionManager.h"

class AppSessionManager : public SessionManager
{
    Q_OBJECT

public:
    AppSessionManager(const QUrl &url);

    int getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes);
    int getResources(QnApiResourceResponsePtr& resources);

    int getCameras(QnApiCameraResponsePtr& scheduleTasks, const QnId& mediaServerId);
    int getStorages(QnApiStorageResponsePtr& resources);

    int addServer(const ::xsd::api::servers::Server&, QnApiServerResponsePtr& servers);
    int addCamera(const ::xsd::api::cameras::Camera&, QnApiCameraResponsePtr& cameras);
    int addStorage(const ::xsd::api::storages::Storage&);

private:
    template<class Container>
    int addObjects(const QString& objectName, const Container& objectsIn, QSharedPointer<Container>& objectsOut,
                   void (*saveFunction) (std::ostream&,
                                         const Container&,
                                         const ::xml_schema::namespace_infomap&,
                                         const ::std::string& e,
                                         xml_schema::flags),
                   std::auto_ptr<Container> (*loadFunction)(std::istream&, xml_schema::flags, const xml_schema::properties&))
    {
        std::ostringstream os;

        saveFunction(os, objectsIn, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

        QByteArray reply;

        int status = addObject(objectName, os.str().c_str(), reply);
        if (status == 0)
        {
            QTextStream stream(reply);
            return loadObjects(stream.device(), objectsOut, loadFunction);
        }

        return status;
    }

    template<class Container>
    int getObjects(const QString& objectName, const QString& args, QSharedPointer<Container>& objects,
                   std::auto_ptr<Container> (*loadFunction)(std::istream&, xml_schema::flags, const xml_schema::properties&) )
    {
        QByteArray reply;

        QString request;

        if (args.isEmpty())
            request = objectName;
        else
            request = QString("%1/%2").arg(objectName).arg(args);

        int status = sendGetRequest(request, reply);
        if (status == 0)
        {
            QTextStream stream(reply);
            return loadObjects(stream.device(), objects, loadFunction);
        }

        return status;
    }

    template<class Container>
    int loadObjects(QIODevice* device, QSharedPointer<Container>& objects,
                   std::auto_ptr<Container> (*loadFunction)(std::istream&, xml_schema::flags, const xml_schema::properties&) )
    {
        try
        {
            QStdIStream is(device);
            objects = QSharedPointer<Container>(loadFunction (is, XSD_FLAGS, xml_schema::properties ()).release());
        }
        catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }

        return 0;
    }

    int addObject(const QString& objectName, const QByteArray& body, QByteArray& response);

private:
    static const unsigned long XSD_FLAGS = xml_schema::flags::dont_initialize | xml_schema::flags::dont_validate;
};

#endif // _APP_SESSION_MANAGER_H
