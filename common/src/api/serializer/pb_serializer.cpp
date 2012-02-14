#include <sstream>

#include "camera.pb.h"
#include "server.pb.h"
#include "user.pb.h"
#include "resource.pb.h"
#include "resourceType.pb.h"

#include "api/QStdIStream.h"

#include "pb_serializer.h"

namespace {

typedef google::protobuf::RepeatedPtrField<proto::pb::Camera> PbCameraList;
typedef google::protobuf::RepeatedPtrField<proto::pb::Server>         PbServerList;
typedef google::protobuf::RepeatedPtrField<proto::pb::Layout>         PbLayoutList;
typedef google::protobuf::RepeatedPtrField<proto::pb::User>           PbUserList;
typedef google::protobuf::RepeatedPtrField<proto::pb::ResourceType>   PbResourceTypeList;

template<class T>
void parseCameras(QList<T>& cameras, const PbCameraList& pb_cameras, QnResourceFactory& resourceFactory)
{
    for (PbCameraList::const_iterator ci = pb_cameras.begin(); ci != pb_cameras.end(); ++ci)
    {
        const proto::pb::Camera& pb_camera = *ci;

        QnResourceParameters parameters;
        parameters["id"] = QString::number(pb_camera.id());
        parameters["name"] = pb_camera.name().c_str();
        parameters["url"] = pb_camera.url().c_str();
        parameters["mac"] = pb_camera.mac().c_str();
        parameters["login"] = pb_camera.login().c_str();
        parameters["password"] = pb_camera.password().c_str();

        if (pb_camera.has_status())
            parameters["status"] = (pb_camera.status() == proto::pb::Camera_Status_Active) ? "A" : "I";

        parameters["parentId"] = QString::number(pb_camera.parentid());

        QnResourcePtr cameraBase = resourceFactory.createResource(pb_camera.typeid_(), parameters);
        if (cameraBase.isNull())
            continue;

        QnVirtualCameraResourcePtr camera = cameraBase.dynamicCast<QnVirtualCameraResource>();
        if (camera.isNull())
            continue;

        camera->setAuth(pb_camera.login().c_str(), pb_camera.password().c_str());

        if (pb_camera.has_region())
        {
            QRegion region;
            parseRegion(region, pb_camera.region().c_str());
            camera->setMotionMask(region);
        }

        if (pb_camera.scheduletask_size())
        {
                QnScheduleTaskList scheduleTasks;

                for (int j = 0; j < pb_camera.scheduletask_size(); j++)
                {
                    const proto::pb::Camera_ScheduleTask& pb_scheduleTask = pb_camera.scheduletask(j);

                    QnScheduleTask scheduleTask(pb_scheduleTask.id(),
                                                pb_camera.id(),
                                                pb_scheduleTask.dayofweek(),
                                                pb_scheduleTask.starttime(),
                                                pb_scheduleTask.endtime(),
                                                (QnScheduleTask::RecordingType) pb_scheduleTask.recordtype(),
                                                pb_scheduleTask.beforethreshold(),
                                                pb_scheduleTask.afterthreshold(),
                                                (QnStreamQuality) pb_scheduleTask.streamquality(),
                                                pb_scheduleTask.fps(),
                                                pb_scheduleTask.dorecordaudio()
                                               );

                    scheduleTasks.append(scheduleTask);
                }

                camera->setScheduleTasks(scheduleTasks);
        }

        cameras.append(camera);
    }
}

template<class T>
void parseServers(QList<T> &servers, const PbServerList& pb_servers)
{
    for (PbServerList::const_iterator ci = pb_servers.begin(); ci != pb_servers.end(); ++ci)
    {
        const proto::pb::Server& pb_server = *ci;

        QnVideoServerResourcePtr server(new QnVideoServerResource());
        server->setId(pb_server.id());
        server->setName(pb_server.name().c_str());
        server->setUrl(pb_server.url().c_str());
        server->setGuid(pb_server.guid().c_str());
        server->setApiUrl(pb_server.apiurl().c_str());

        if (pb_server.storage_size() > 0)
        {
            QnStorageResourceList storages;

            for (int j = 0; j < pb_server.storage_size(); j++)
            {
                const proto::pb::Server_Storage& pb_storage = pb_server.storage(j);

                QnStorageResourcePtr storage(new QnStorageResource());
                storage->setId(pb_storage.id());
                storage->setParentId(pb_server.id());
                storage->setName(pb_storage.name().c_str());

                storage->setUrl(pb_storage.url().c_str());
                storage->setSpaceLimit(pb_storage.spacelimit());

                storages.append(storage);
            }

            server->setStorages(storages);
        }

        servers.append(server);
    }
}

template<class T>
void parseLayouts(QList<T>& layouts, const PbLayoutList& pb_layouts)
{
    for (PbLayoutList::const_iterator ci = pb_layouts.begin(); ci != pb_layouts.end(); ++ci)
    {
        const proto::pb::Layout& pb_layout = *ci;

        QnLayoutResourcePtr layout(new QnLayoutResource());

        QnResourceParameters parameters;
        if (pb_layout.has_id())
        {
            layout->setId(pb_layout.id());
            parameters["id"] = QString::number(pb_layout.id());
        }

        layout->setParentId(pb_layout.parentid());
        parameters["parentId"] = QString::number(pb_layout.parentid());

        layout->setName(pb_layout.name().c_str());
        parameters["name"] = pb_layout.name().c_str();

        if (pb_layout.item_size() > 0)
        {
            QnLayoutItemDataList items;

            for (int j = 0; j < pb_layout.item_size(); j++)
            {
                const proto::pb::Layout_Item& pb_item = pb_layout.item(j);

                QnLayoutItemData itemData;
                itemData.resourceId = pb_item.resourceid();
                itemData.uuid = QUuid(pb_item.uuid().c_str());
                itemData.flags = pb_item.flags();
                itemData.combinedGeometry.setLeft(pb_item.left());
                itemData.combinedGeometry.setTop(pb_item.top());
                itemData.combinedGeometry.setRight(pb_item.right());
                itemData.combinedGeometry.setBottom(pb_item.bottom());
                itemData.rotation = pb_item.rotation();

                items.append(itemData);
            }

            layout->setItems(items);
        }


        layouts.append(layout);
    }
}

template<class T>
void parseUsers(QList<T>& users, const PbUserList& pb_users)
{
    for (PbUserList::const_iterator ci = pb_users.begin(); ci != pb_users.end(); ++ci)
    {
        const proto::pb::User& pb_user = *ci;

        QnUserResourcePtr user(new QnUserResource());
        QnResourceParameters parameters;

        if (pb_user.has_id())
        {
            user->setId(pb_user.id());
            parameters["id"] = QString::number(pb_user.id());
        }

        parameters["name"] = pb_user.name().c_str();

        user->setName(pb_user.name().c_str());

        if (pb_user.layout_size() > 0)
        {
            QnLayoutResourceList layouts;
            parseLayouts(layouts, pb_user.layout());
            user->setLayouts(layouts);
        }

        users.append(user);
    }
}

void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const PbResourceTypeList& pb_resourceTypes)
{
    for (PbResourceTypeList::const_iterator ci = pb_resourceTypes.begin (); ci != pb_resourceTypes.end (); ++ci)
    {
        const proto::pb::ResourceType& pb_resourceType = *ci;

        QnResourceTypePtr resourceType(new QnResourceType());

        resourceType->setId(pb_resourceType.id());
        resourceType->setName(pb_resourceType.name().c_str());
        if (pb_resourceType.has_manufacture())
            resourceType->setManufacture(pb_resourceType.manufacture().c_str());

        if (pb_resourceType.parentid_size() > 0)
        {
            bool firstParent = true;
            for (int i = 0; i < pb_resourceType.parentid_size(); i++)
            {
                qint32 id = pb_resourceType.parentid(i);
                if (i == 0)
                    resourceType->setParentId(id);
                else
                    resourceType->addAdditionalParent(id);
            }
        }

        if (pb_resourceType.propertytype_size() > 0)
        {
            for (int i = 0; i < pb_resourceType.propertytype_size(); i++)
            {
                const proto::pb::PropertyType& pb_propertyType = pb_resourceType.propertytype(i);

                QnParamTypePtr param(new QnParamType());
                param->name = pb_propertyType.name().c_str();
                param->type = (QnParamType::DataType)pb_propertyType.type();

                if (pb_propertyType.has_min())
                    param->min_val = pb_propertyType.min();

                if (pb_propertyType.has_max())
                    param->max_val = pb_propertyType.max();

                if (pb_propertyType.has_step())
                    param->step = pb_propertyType.step();

                if (pb_propertyType.has_values())
                {
                    QString values = pb_propertyType.values().c_str();
                    foreach(QString val, values.split(QLatin1Char(',')))
                        param->possible_values.push_back(val.trimmed());
                }

                if (pb_propertyType.has_ui_values())
                {
                    QString ui_values = pb_propertyType.ui_values().c_str();
                    foreach(QString val, ui_values.split(QLatin1Char(',')))
                        param->ui_possible_values.push_back(val.trimmed());
                }

                if (pb_propertyType.has_default_value())
                    param->default_value = pb_propertyType.default_value().c_str();

                if (pb_propertyType.has_nethelper())
                    param->paramNetHelper = pb_propertyType.nethelper().c_str();

                if (pb_propertyType.has_group())
                    param->group = pb_propertyType.group().c_str();

                if (pb_propertyType.has_sub_group())
                    param->subgroup = pb_propertyType.sub_group().c_str();

                if (pb_propertyType.has_description())
                    param->description = pb_propertyType.description().c_str();

                if (pb_propertyType.has_ui())
                    param->ui = pb_propertyType.ui();

                if (pb_propertyType.has_readonly())
                    param->isReadOnly = pb_propertyType.readonly();

                resourceType->addParamType(param);
            }
        }

        resourceTypes.append(resourceType);
    }
}

}

void QnApiPbSerializer::deserializeCameras(QnVirtualCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    proto::pb::Cameras pb_cameras;
    if (!pb_cameras.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeCameras(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseCameras(cameras, pb_cameras.camera(), resourceFactory);
}

void QnApiPbSerializer::deserializeServers(QnVideoServerResourceList& servers, const QByteArray& data)
{
    proto::pb::Servers pb_servers;
    if (!pb_servers.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeServers(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseServers(servers, pb_servers.server());
}

void QnApiPbSerializer::deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data)
{
    proto::pb::Layouts pb_layouts;
    if (!pb_layouts.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeLayouts(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseLayouts(layouts, pb_layouts.layout());
}

void QnApiPbSerializer::deserializeUsers(QnUserResourceList& users, const QByteArray& data)
{
    proto::pb::Users pb_users;
    if (!pb_users.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeUsers(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseUsers(users, pb_users.user());
}

void QnApiPbSerializer::deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    proto::pb::Resources pb_resources;
    if (!pb_resources.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeCameras(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseCameras(resources, pb_resources.camera(), resourceFactory);
    parseServers(resources, pb_resources.server());
    parseUsers(resources, pb_resources.user());
}

void QnApiPbSerializer::deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data)
{
    proto::pb::ResourceTypes pb_resourceTypes;
    if (!pb_resourceTypes.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeCameras(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseResourceTypes(resourceTypes, pb_resourceTypes.resourcetype());
}

void QnApiPbSerializer::serializeServer(const QnVideoServerResourcePtr& serverPtr, QByteArray& data)
{
    proto::pb::Servers pb_servers;

    proto::pb::Server& pb_server = *pb_servers.add_server();

    pb_server.set_id(serverPtr->getId().toString().toInt());
    pb_server.set_name(serverPtr->getName().toStdString());
    pb_server.set_url(serverPtr->getUrl().toStdString());
    pb_server.set_guid(serverPtr->getGuid().toStdString());
    pb_server.set_apiurl(serverPtr->getApiUrl().toStdString());

    if (!serverPtr->getStorages().isEmpty()) {
        foreach (const QnStorageResourcePtr& storagePtr, serverPtr->getStorages()) {
            proto::pb::Server_Storage& pb_storage = *pb_server.add_storage();

            pb_storage.set_id(storagePtr->getId().toString().toInt());
            pb_storage.set_name(storagePtr->getName().toStdString());
            pb_storage.set_url(storagePtr->getUrl().toStdString());
            pb_storage.set_spacelimit(storagePtr->getSpaceLimit());
        }
    }

    std::ostringstream os;
    pb_servers.SerializeToOstream(&os);

    data = os.str().c_str();
}

void QnApiPbSerializer::serializeUser(const QnUserResourcePtr& userPtr, QByteArray& data)
{
    proto::pb::Users pb_users;
    proto::pb::User& pb_user = *pb_users.add_user();

    pb_user.set_name(userPtr->getName().toStdString());

    if (userPtr->getId().isValid())
        pb_user.set_id(userPtr->getId().toString().toInt());

    pb_user.set_password(userPtr->getPassword().toStdString());

    foreach(const QnLayoutResourcePtr& layoutIn, userPtr->getLayouts()) {
        proto::pb::Layout& pb_layout = *pb_user.add_layout();
        pb_layout.set_name(layoutIn->getName().toStdString());
        pb_layout.set_parentid(userPtr->getId().toString().toInt());

        if (!layoutIn->getItems().isEmpty()) {
            foreach(const QnLayoutItemData& itemIn, layoutIn->getItems()) {
                proto::pb::Layout_Item& pb_item = *pb_layout.add_item();

                pb_item.set_resourceid(itemIn.resourceId.toString().toInt());
                pb_item.set_uuid(itemIn.uuid.toString().toStdString());
                pb_item.set_flags(itemIn.flags);
                pb_item.set_left(itemIn.combinedGeometry.left());
                pb_item.set_top(itemIn.combinedGeometry.top());
                pb_item.set_right(itemIn.combinedGeometry.right());
                pb_item.set_bottom(itemIn.combinedGeometry.bottom());
                pb_item.set_rotation(itemIn.rotation);
            }
        }
    }

    std::ostringstream os;
    pb_users.SerializeToOstream(&os);

    data = os.str().c_str();
}

void QnApiPbSerializer::serializeCamera(const QnVirtualCameraResourcePtr& cameraPtr, QByteArray& data)
{
    proto::pb::Cameras pb_cameras;
    proto::pb::Camera& pb_camera = *pb_cameras.add_camera();

    pb_camera.set_id(cameraPtr->getId().toString().toInt());
    pb_camera.set_parentid(cameraPtr->getParentId().toString().toInt());
    pb_camera.set_name(cameraPtr->getName().toStdString());
    pb_camera.set_typeid_(cameraPtr->getTypeId().toString().toInt());
    pb_camera.set_url(cameraPtr->getUrl().toStdString());
    pb_camera.set_mac(cameraPtr->getMAC().toString().toStdString());
    pb_camera.set_login(cameraPtr->getAuth().user().toStdString());
    pb_camera.set_password(cameraPtr->getAuth().password().toStdString());
    pb_camera.set_status(cameraPtr->getStatus() == QnResource::Online ? proto::pb::Camera_Status_Active : proto::pb::Camera_Status_Inactive);
    pb_camera.set_region(serializeRegion(cameraPtr->getMotionMask()).toStdString());

    foreach(const QnScheduleTask& scheduleTaskIn, cameraPtr->getScheduleTasks()) {
        proto::pb::Camera_ScheduleTask& pb_scheduleTask = *pb_camera.add_scheduletask();

        pb_scheduleTask.set_starttime(scheduleTaskIn.getStartTime());
        pb_scheduleTask.set_endtime(scheduleTaskIn.getEndTime());
        pb_scheduleTask.set_dorecordaudio(scheduleTaskIn.getDoRecordAudio());
        pb_scheduleTask.set_recordtype(scheduleTaskIn.getRecordingType());
        pb_scheduleTask.set_dayofweek(scheduleTaskIn.getDayOfWeek());
        pb_scheduleTask.set_beforethreshold(scheduleTaskIn.getBeforeThreshold());
        pb_scheduleTask.set_afterthreshold(scheduleTaskIn.getAfterThreshold());
        pb_scheduleTask.set_streamquality((proto::pb::Camera_Quality)scheduleTaskIn.getStreamQuality());
        pb_scheduleTask.set_fps(scheduleTaskIn.getFps());
    }

    std::ostringstream os;
    pb_cameras.SerializeToOstream(&os);

    data = os.str().c_str();
}
