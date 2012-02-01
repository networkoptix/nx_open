#include "xml_serializer.h"

#include "api/QStdIStream.h"

#include "xsd_resourceTypes.h"
#include "xsd_resources.h"
#include "xsd_resourcesEx.h"
#include "xsd_layouts.h"
#include "xsd_cameras.h"
#include "xsd_servers.h"
#include "xsd_users.h"
#include "xsd_events.h"
#include "xsd_storages.h"
#include "xsd_scheduleTasks.h"

// for appServer methods

typedef xsd::cxx::tree::sequence<xsd::api::resourceTypes::ResourceType>  QnApiResourceTypes;
typedef xsd::cxx::tree::sequence<xsd::api::resources::Resource>          QnApiResources;

typedef xsd::cxx::tree::sequence<xsd::api::cameras::Camera>              QnApiCameras;
typedef xsd::cxx::tree::sequence<xsd::api::layouts::Layout>              QnApiLayouts;
typedef xsd::cxx::tree::sequence<xsd::api::users::User>                  QnApiUsers;
typedef xsd::cxx::tree::sequence<xsd::api::servers::Server>              QnApiServers;
typedef xsd::cxx::tree::sequence<xsd::api::storages::Storage>            QnApiStorages;
typedef xsd::cxx::tree::sequence<xsd::api::scheduleTasks::ScheduleTask>  QnApiScheduleTasks;

static const unsigned long XSD_FLAGS = xml_schema::flags::dont_initialize | xml_schema::flags::dont_validate;

namespace {
    typedef xsd::cxx::tree::sequence<xsd::api::scheduleTasks::ScheduleTask>  QnApiScheduleTasks;

    void parseRegion(QRegion& region, const QString& regionString)
    {
        foreach (QString rectString, regionString.split(';'))
        {
            QRect rect;
            QStringList rectList = rectString.split(',');

            if (rectList.size() == 4)
            {
                rect.setLeft(rectList[0].toInt());
                rect.setTop(rectList[1].toInt());
                rect.setWidth(rectList[2].toInt());
                rect.setHeight(rectList[3].toInt());
            }

            region += rect;
        }
    }

    void parseScheduleTasks(QnScheduleTaskList& scheduleTasks, const QnApiScheduleTasks& xsdScheduleTasks)
    {
        using xsd::api::scheduleTasks::ScheduleTasks;
        using xsd::api::resourceTypes::ParentIDs;
        using xsd::api::resourceTypes::PropertyTypes;
        using xsd::api::resourceTypes::ParamType;

        for (ScheduleTasks::scheduleTask_const_iterator i (xsdScheduleTasks.begin()); i != xsdScheduleTasks.end(); ++i)
        {
            QnScheduleTask scheduleTask(       i->id().present() ? (*(i->id())).c_str() : "",
                                               i->sourceId().present() ? (*(i->sourceId())).c_str() : "",
                                               i->dayOfWeek(),
                                               i->startTime(),
                                               i->endTime(),
                                               (QnScheduleTask::RecordingType) i->recordType(),
                                               i->beforeThreshold(),
                                               i->afterThreshold(),
                                               (QnStreamQuality)i->streamQuality(),
                                               i->fps(),
                                               i->doRecordAudio()
                                            );

            scheduleTasks.append(scheduleTask);
        }
    }

    void parseStorages(QnStorageList& storages, const QnApiStorages& xsdStorages)
    {
        using xsd::api::storages::Storages;
        using xsd::api::resourceTypes::ParentIDs;
        using xsd::api::resourceTypes::PropertyTypes;
        using xsd::api::resourceTypes::ParamType;

        for (Storages::storage_const_iterator i (xsdStorages.begin()); i != xsdStorages.end(); ++i)
        {
            QnStoragePtr storage(new QnStorage());
            storage->setId(i->id().c_str());
            storage->setName(i->name().c_str());
            storage->setTypeId(i->typeId().c_str());
            storage->setParentId(i->parentId().c_str());

            storage->setUrl(i->url().c_str());

            storage->setSpaceLimit(i->spaceLimit());
            storage->setMaxStoreTime(i->time());

            storages.append(storage);
        }
    }

    template<class T>
    void parseCameras(QList<T>& cameras, const QnApiCameras& xsdCameras, QnResourceFactory& resourceFactory)
    {
        using xsd::api::cameras::Cameras;
        using xsd::api::resources::Properties;
        using xsd::api::resourceTypes::ParentIDs;
        using xsd::api::resourceTypes::PropertyTypes;
        using xsd::api::resourceTypes::ParamType;

        for (Cameras::camera_const_iterator i (xsdCameras.begin()); i != xsdCameras.end(); ++i)
        {
            QnResourceParameters parameters;
            parameters["id"] = i->id().c_str();
            parameters["name"] = i->name().c_str();
            parameters["url"] = i->url().c_str();
            parameters["mac"] = i->mac().c_str();
            parameters["login"] = i->login().c_str();
            parameters["password"] = i->password().c_str();

            if (i->status().present())
                parameters["status"] = (*i->status()).c_str();

            if (i->parentID().present())
                parameters["parentId"] = (*i->parentID()).c_str();

            QnResourcePtr cameraBase = resourceFactory.createResource(i->typeId().c_str(), parameters);
            if (cameraBase.isNull())
                continue;

            QnVirtualCameraResourcePtr camera = cameraBase.dynamicCast<QnVirtualCameraResource>();
            if (camera.isNull())
                continue;

            camera->setAuth(i->login().c_str(), i->password().c_str());

            if (i->region().present())
            {
                QRegion region;
                parseRegion(region, (*i->region()).c_str());
                camera->setMotionMask(region);
            }

            const QnParamList& paramList = camera->getResourceParamList();

            if (i->properties().present())
            {
                const Properties& properties = *(i->properties());
                for (Properties::property_const_iterator j(properties.property().begin()); j != properties.property().end(); ++j)
                {
                    // TODO add parameter to paramList here
                }
            }

            if (i->scheduleTasks().present())
            {
                    QnScheduleTaskList scheduleTaskList;
                    parseScheduleTasks(scheduleTaskList, (*(i->scheduleTasks())).scheduleTask());
                    camera->setScheduleTasks(scheduleTaskList);
            }

            cameras.append(camera);
        }
    }

    template<class T>
    void parseServers(QList<T> &servers, const QnApiServers &xsdServers)
    {
        using xsd::api::servers::Servers;
        using xsd::api::resourceTypes::ParentIDs;
        using xsd::api::resourceTypes::PropertyTypes;
        using xsd::api::resourceTypes::ParamType;

        for (Servers::server_const_iterator i (xsdServers.begin()); i != xsdServers.end(); ++i)
        {
            QnVideoServerPtr server(new QnVideoServer());
            server->setName(i->name().c_str());
            server->setId(i->id().c_str());
            server->setUrl(i->url().c_str());
            server->setGuid(i->guid().c_str());
            server->setApiUrl(i->apiUrl().c_str());

            servers.append(server);
        }
    }

    template<class T>
    void parseLayouts(QList<T>& layouts, const QnApiLayouts& xsdLayouts)
    {
        using xsd::api::layouts::Layouts;
        using xsd::api::layouts::Items;

        using xsd::api::resourceTypes::ParentIDs;
        using xsd::api::resourceTypes::PropertyTypes;
        using xsd::api::resourceTypes::ParamType;

        for (Layouts::layout_const_iterator i (xsdLayouts.begin()); i != xsdLayouts.end(); ++i)
        {
            QnLayoutDataPtr layout(new QnLayoutData());

            QnResourceParameters parameters;
            if (i->id().present())
            {
                layout->setId((*(i->id())).c_str());
                parameters["id"] = (*(i->id())).c_str();
            }

            layout->setParentId(i->parentId().c_str());
            parameters["parentId"] = i->parentId().c_str();

            layout->setName(i->name().c_str());
            parameters["name"] = i->name().c_str();

            if (i->items().present())
            {
                QnLayoutItemDataList items;

                const Items::item_sequence& xsdItems = (*(i->items())).item();
                for (Items::item_const_iterator ci(xsdItems.begin()); ci != xsdItems.end(); ++ci)
                {
                    QnLayoutItemData itemData;
                    itemData.resourceId = ci->resourceId().c_str();
                    itemData.flags = ci->flags();
                    itemData.combinedGeometry.setLeft(ci->left());
                    itemData.combinedGeometry.setTop(ci->top());
                    itemData.combinedGeometry.setRight(ci->right());
                    itemData.combinedGeometry.setBottom(ci->bottom());
                    itemData.rotation = ci->rotation();

                    items.append(itemData);
                }

                layout->setItems(items);
            }


            layouts.append(layout);
        }
    }

    template<class T>
    void parseUsers(QList<T>& users, const QnApiUsers& xsdUsers)
    {
        using xsd::api::users::Users;
        using xsd::api::layouts::Layouts;

        using xsd::api::resourceTypes::ParentIDs;
        using xsd::api::resourceTypes::PropertyTypes;
        using xsd::api::resourceTypes::ParamType;

        for (Users::user_const_iterator i (xsdUsers.begin()); i != xsdUsers.end(); ++i)
        {
            QnUserResourcePtr user(new QnUserResource());
            QnResourceParameters parameters;

            if (i->id().present())
            {
                user->setId((*i->id()).c_str());
                parameters["id"] = (*i->id()).c_str();
            }

            parameters["name"] = i->name().c_str();

            user->setName(i->name().c_str());

            if (i->layouts().present())
            {
                const Layouts::layout_sequence& xsdLayouts = (*(i->layouts())).layout();

                QnLayoutDataList layouts;
                parseLayouts(layouts, xsdLayouts);
                user->setLayouts(layouts);
            }

            users.append(user);
        }
    }

    void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const QnApiResourceTypes& xsdResourceTypes)
    {
        using xsd::api::resourceTypes::ResourceTypes;
        using xsd::api::resourceTypes::ParentIDs;
        using xsd::api::resourceTypes::PropertyTypes;
        using xsd::api::resourceTypes::ParamType;

        for (ResourceTypes::resourceType_const_iterator i (xsdResourceTypes.begin ());
                 i != xsdResourceTypes.end ();
                 ++i)
        {
            QnResourceTypePtr resourceType(new QnResourceType());

            resourceType->setId(i->id().c_str());
            resourceType->setName(i->name().c_str());
            if (i->manufacture().present())
                resourceType->setManufacture(i->manufacture()->c_str());

            if (i->parentIDs().present())
            {
                bool firstParent = true;
                for (ParentIDs::parentID_const_iterator parentsIter (i->parentIDs ()->parentID().begin ());
                         parentsIter != i->parentIDs ()->parentID().end ();
                         ++parentsIter)
                {
                    if (firstParent)
                    {
                        resourceType->setParentId(parentsIter->c_str());
                        firstParent = false;
                    }
                    else
                    {
                        resourceType->addAdditionalParent(parentsIter->c_str());
                    }
                }
            }

            if (i->propertyTypes().present())
            {
                for (PropertyTypes::propertyType_const_iterator propertyTypesIter (i->propertyTypes()->propertyType().begin ());
                         propertyTypesIter != i->propertyTypes()->propertyType().end ();
                         ++propertyTypesIter)
                {
                    QnParamTypePtr param(new QnParamType());
                    param->name = propertyTypesIter->name().c_str();
                    param->type = (QnParamType::DataType)(ParamType::value)propertyTypesIter->type();

                    if (propertyTypesIter->min().present())
                        param->min_val = *propertyTypesIter->min();

                    if (propertyTypesIter->max().present())
                        param->max_val = *propertyTypesIter->max();

                    if (propertyTypesIter->step().present())
                        param->step = *propertyTypesIter->step();

                    if (propertyTypesIter->values().present())
                    {
                        QString values = (*propertyTypesIter->values()).c_str();
                        foreach(QString val, values.split(QLatin1Char(',')))
                            param->possible_values.push_back(val.trimmed());
                    }

                    if (propertyTypesIter->ui_values().present())
                    {
                        QString ui_values = (*propertyTypesIter->ui_values()).c_str();
                        foreach(QString val, ui_values.split(QLatin1Char(',')))
                            param->ui_possible_values.push_back(val.trimmed());
                    }

                    if (propertyTypesIter->default_value().present())
                        param->default_value = (*propertyTypesIter->default_value()).c_str();

                    if (propertyTypesIter->netHelper().present())
                        param->paramNetHelper = (*propertyTypesIter->netHelper()).c_str();

                    if (propertyTypesIter->group().present())
                        param->group = (*propertyTypesIter->group()).c_str();

                    if (propertyTypesIter->sub_group().present())
                        param->subgroup = (*propertyTypesIter->sub_group()).c_str();

                    if (propertyTypesIter->description().present())
                        param->description = (*propertyTypesIter->description()).c_str();

                    if (propertyTypesIter->ui().present())
                        param->ui = *propertyTypesIter->ui();

                    if (propertyTypesIter->readonly().present())
                        param->isReadOnly = *propertyTypesIter->readonly();

                    resourceType->addParamType(param);
                }
            }

            resourceTypes.append(resourceType);
        }
    }

}

void QnApiXmlSerializer::deserializeStorages(QnStorageList& storages, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    QByteArray errorString;

    try {
        QTextStream stream(data);
        QStdIStream is(stream.device());

        std::auto_ptr<xsd::api::storages::Storages> xsdStorages = xsd::api::storages::storages (is, XSD_FLAGS, xml_schema::properties ());

        parseStorages(storages, xsdStorages->storage());
    }
    catch (const xml_schema::exception& e) {
        errorString += "\nAppSessionManager::loadObjects(): ";
        errorString += e.what();

        qDebug(e.what());
        throw QnSerializeException(errorString);
    }
}

void QnApiXmlSerializer::deserializeCameras(QnVirtualCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    QByteArray errorString;

    try {
        QTextStream stream(data);
        QStdIStream is(stream.device());
        std::auto_ptr<xsd::api::cameras::Cameras> xsdCameras = xsd::api::cameras::cameras (is, XSD_FLAGS, xml_schema::properties ());

        parseCameras(cameras, xsdCameras->camera(), resourceFactory);
    }
    catch (const xml_schema::exception& e) {
        errorString += "\nAppSessionManager::loadObjects(): ";
        errorString += e.what();

        qDebug(e.what());
        throw QnSerializeException(errorString);
    }
}

void QnApiXmlSerializer::deserializeLayouts(QnLayoutDataList& layouts, const QByteArray& data)
{
    QByteArray errorString;

    try {
        QTextStream stream(data);
        QStdIStream is(stream.device());
        std::auto_ptr<xsd::api::layouts::Layouts> xsdLayouts = xsd::api::layouts::layouts (is, XSD_FLAGS, xml_schema::properties ());

        parseLayouts(layouts, xsdLayouts->layout());
    }
    catch (const xml_schema::exception& e) {
        errorString += "\nAppSessionManager::loadObjects(): ";
        errorString += e.what();

        qDebug(e.what());
        throw QnSerializeException(errorString);
    }
}

void QnApiXmlSerializer::deserializeUsers(QnUserResourceList& users, const QByteArray& data)
{
    QByteArray errorString;

    try {
        QTextStream stream(data);
        QStdIStream is(stream.device());
        std::auto_ptr<xsd::api::users::Users> xsdUsers = xsd::api::users::users (is, XSD_FLAGS, xml_schema::properties ());

        parseUsers(users, xsdUsers->user());
    }
    catch (const xml_schema::exception& e) {
        errorString += "\nAppSessionManager::loadObjects(): ";
        errorString += e.what();

        qDebug(e.what());
        throw QnSerializeException(errorString);
    }
}

void QnApiXmlSerializer::deserializeServers(QnVideoServerList& servers, const QByteArray& data)
{
    QByteArray errorString;

    try {
        QTextStream stream(data);
        QStdIStream is(stream.device());
        std::auto_ptr<xsd::api::servers::Servers> xsdServers = xsd::api::servers::servers (is, XSD_FLAGS, xml_schema::properties ());

        parseServers(servers, xsdServers->server());
    }
    catch (const xml_schema::exception& e)
    {
        errorString += "\nAppSessionManager::loadObjects(): ";
        errorString += e.what();

        qDebug(e.what());
        throw QnSerializeException(errorString);
    }
}

void QnApiXmlSerializer::deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    QByteArray errorString;

    try {
        QTextStream stream(data);
        QStdIStream is(stream.device());
        std::auto_ptr<xsd::api::resourcesEx::Resources> xsdResources = xsd::api::resourcesEx::resourcesEx(is, XSD_FLAGS, xml_schema::properties ());

        parseCameras(resources, xsdResources->cameras().camera(), resourceFactory);
        parseServers(resources, xsdResources->servers().server());
        parseLayouts(resources, xsdResources->layouts().layout());
        // parseUsers(resources, xsdResources->users().user());
    }
    catch (const xml_schema::exception& e) {
        errorString += "\nAppSessionManager::loadObjects(): ";
        errorString += e.what();

        qDebug(e.what());
        throw QnSerializeException(errorString);
    }
}

void QnApiXmlSerializer::deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data)
{
    QByteArray errorString;

    try {
        QTextStream stream(data);
        QStdIStream is(stream.device());
        std::auto_ptr<xsd::api::resourceTypes::ResourceTypes> xsdResourceTypes = xsd::api::resourceTypes::resourceTypes (is, XSD_FLAGS, xml_schema::properties ());

        parseResourceTypes(resourceTypes, xsdResourceTypes->resourceType());
    }
    catch (const xml_schema::exception& e) {
        errorString += "\nAppSessionManager::loadObjects(): ";
        errorString += e.what();

        qDebug(e.what());
        throw QnSerializeException(errorString);
    }
}

void QnApiXmlSerializer::serializeServer(const QnVideoServerPtr& serverPtr, QByteArray& data)
{
    xsd::api::servers::Server server(
                   serverPtr->getId().toString().toStdString(),
                   serverPtr->getName().toStdString(),
                   serverPtr->getTypeId().toString().toStdString(),
                   serverPtr->getUrl().toStdString(),
                   serverPtr->getGuid().toStdString(),
                   serverPtr->getApiUrl().toStdString());

    std::ostringstream os;
    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);

    ::xsd::api::servers::servers(os, servers, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

    data = os.str().c_str();
}

void QnApiXmlSerializer::serializeUser(const QnUserResourcePtr& userPtr, QByteArray& data)
{
    xsd::api::users::User user(
                   userPtr->getName().toStdString(),
                   userPtr->getTypeId().toString().toStdString());

    if (userPtr->getId().isValid())
        user.id(userPtr->getId().toString().toStdString());

    user.status("A");
    user.password(userPtr->getPassword().toStdString());

    xsd::api::layouts::Layouts layouts;
    foreach(const QnLayoutDataPtr& layoutIn, userPtr->getLayouts()) {
        xsd::api::layouts::Layout layout(layoutIn->getName().toStdString(), layoutIn->getParentId().toString().toStdString());

        if (!layoutIn->getItems().isEmpty()) {
            xsd::api::layouts::Items items;

            foreach(const QnLayoutItemData& itemIn, layoutIn->getItems()) {
                xsd::api::layouts::Item item(itemIn.resourceId.toString().toStdString(),
                                             itemIn.flags,
                                             itemIn.combinedGeometry.left(),
                                             itemIn.combinedGeometry.top(),
                                             itemIn.combinedGeometry.right(),
                                             itemIn.combinedGeometry.bottom(),
                                             itemIn.rotation);
                items.item().push_back(item);
            }

            layout.items(items);
        }

        layouts.layout().push_back(layout);
    }

    user.layouts(layouts);

    std::ostringstream os;
    ::xsd::api::users::Users users;
    users.user().push_back(user);

    ::xsd::api::users::users(os, users, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

    data = os.str().c_str();
}

void QnApiXmlSerializer::serializeCamera(const QnVirtualCameraResourcePtr& cameraPtr, QByteArray& data)
{
    xsd::api::cameras::Camera camera(cameraPtr->getId().toString().toStdString(),
                                     cameraPtr->getName().toStdString(),
                                     cameraPtr->getTypeId().toString().toStdString(),
                                     cameraPtr->getUrl().toStdString(),
                                     cameraPtr->getMAC().toString().toStdString(),
                                     cameraPtr->getAuth().user().toStdString(),
                                     cameraPtr->getAuth().password().toStdString());

    xsd::api::scheduleTasks::ScheduleTasks scheduleTasks;
    foreach(const QnScheduleTask& scheduleTaskIn, cameraPtr->getScheduleTasks()) {
        xsd::api::scheduleTasks::ScheduleTask scheduleTask(
                                                scheduleTaskIn.getStartTime(),
                                                scheduleTaskIn.getEndTime(),
                                                scheduleTaskIn.getDoRecordAudio(),
                                                scheduleTaskIn.getRecordingType(),
                                                scheduleTaskIn.getDayOfWeek(),
                                                scheduleTaskIn.getBeforeThreshold(),
                                                scheduleTaskIn.getAfterThreshold(),
                                                scheduleTaskIn.getStreamQuality(),
                                                scheduleTaskIn.getFps());

        scheduleTasks.scheduleTask().push_back(scheduleTask);
    }

    camera.scheduleTasks(scheduleTasks);
    camera.parentID(cameraPtr->getParentId().toString().toStdString());
    camera.status((cameraPtr->getStatus() == QnResource::Online) ? "A" : "I");

    const QAuthenticator& auth = cameraPtr->getAuth();

    camera.login(auth.user().toStdString());
    camera.password(auth.user().toStdString());

    std::ostringstream os;

    ::xsd::api::cameras::Cameras cameras;
    cameras.camera().push_back(camera);
    ::xsd::api::cameras::cameras(os, cameras, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

    data = os.str().c_str();
}

void QnApiXmlSerializer::serializeStorage(const QnStoragePtr& storagePtr, QByteArray& data)
{
    xsd::api::storages::Storage storage(storagePtr->getId().toString().toStdString(),
                                         storagePtr->getName().toStdString(),
                                         storagePtr->getUrl().toStdString(),
                                         storagePtr->getTypeId().toString().toStdString(),
                                         storagePtr->getParentId().toString().toStdString(),
                                         storagePtr->getSpaceLimit(),
                                         storagePtr->getMaxStoreTime());

    std::ostringstream os;

    ::xsd::api::storages::Storages storages;
    storages.storage().push_back(storage);
    ::xsd::api::storages::storages(os, storages, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

    data = os.str().c_str();
}

void QnApiXmlSerializer::serialize(const QnResourcePtr& resource, QByteArray& data)
{
    if (resource.dynamicCast<QnStorage>()) {
        serializeStorage(resource.dynamicCast<QnStorage>(), data);
    } else if (resource.dynamicCast<QnVirtualCameraResource>()) {
        serializeCamera(resource.dynamicCast<QnVirtualCameraResource>(), data);
    } else if (resource.dynamicCast<QnVideoServer>()) {
        serializeServer(resource.dynamicCast<QnVideoServer>(), data);
    } else if (resource.dynamicCast<QnUserResource>()) {
        serializeUser(resource.dynamicCast<QnUserResource>(), data);
    }
}

