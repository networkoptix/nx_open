#include "camera.pb.h"
#include "server.pb.h"
#include "user.pb.h"
#include "resource.pb.h"
#include "resourceType.pb.h"
#include "license.pb.h"
#include "cameraServerItem.pb.h"
#include "api/QStdIStream.h"

#include "pb_serializer.h"

namespace {

typedef google::protobuf::RepeatedPtrField<proto::pb::Camera>           PbCameraList;
typedef google::protobuf::RepeatedPtrField<proto::pb::Server>           PbServerList;
typedef google::protobuf::RepeatedPtrField<proto::pb::Layout>           PbLayoutList;
typedef google::protobuf::RepeatedPtrField<proto::pb::User>             PbUserList;
typedef google::protobuf::RepeatedPtrField<proto::pb::ResourceType>     PbResourceTypeList;
typedef google::protobuf::RepeatedPtrField<proto::pb::License>          PbLicenseList;
typedef google::protobuf::RepeatedPtrField<proto::pb::CameraServerItem> PbCameraServerItemList;

QString serializeNetAddrList(const QList<QHostAddress>& netAddrList)
{
    QStringList addListStrings;
    std::transform(netAddrList.begin(), netAddrList.end(), std::back_inserter(addListStrings), std::mem_fun_ref(&QHostAddress::toString));
    return addListStrings.join(";");
}

QHostAddress stringToAddr(const QString& hostStr)
{
    return QHostAddress(hostStr);
}

void deserializeNetAddrList(QList<QHostAddress>& netAddrList, const QString& netAddrListString)
{
    QStringList addListStrings = netAddrListString.split(";");
    std::transform(addListStrings.begin(), addListStrings.end(), std::back_inserter(netAddrList), stringToAddr);
}

template<class T>
void parseCameras(QList<T>& cameras, const PbCameraList& pb_cameras, QnResourceFactory& resourceFactory)
{
    for (PbCameraList::const_iterator ci = pb_cameras.begin(); ci != pb_cameras.end(); ++ci)
    {
        const proto::pb::Camera& pb_camera = *ci;

        QnResourceParameters parameters;
        parameters["id"] = QString::number(pb_camera.id());
        parameters["name"] = QString::fromUtf8(pb_camera.name().c_str());
        parameters["url"] = QString::fromUtf8(pb_camera.url().c_str());
        parameters["mac"] = QString::fromUtf8(pb_camera.mac().c_str());
        parameters["login"] = QString::fromUtf8(pb_camera.login().c_str());
        parameters["password"] = QString::fromUtf8(pb_camera.password().c_str());

        if (pb_camera.has_status())
            parameters["status"] = QString::number((int)pb_camera.status());

		parameters["disabled"] = QString::number((int)pb_camera.disabled());
        parameters["parentId"] = QString::number(pb_camera.parentid());

        QnResourcePtr cameraBase = resourceFactory.createResource(pb_camera.typeid_(), parameters);
        if (cameraBase.isNull())
            continue;

        QnVirtualCameraResourcePtr camera = cameraBase.dynamicCast<QnVirtualCameraResource>();
        if (camera.isNull())
            continue;

        camera->setScheduleDisabled(pb_camera.scheduledisabled());
        camera->setAuth(QString::fromUtf8(pb_camera.login().c_str()), QString::fromUtf8(pb_camera.password().c_str()));
        camera->setMotionType(static_cast<MotionType>(pb_camera.motiontype()));

        if (pb_camera.has_region())
        {
            QList<QnMotionRegion> regions;
            parseMotionRegionList(regions, pb_camera.region().c_str());
            while (regions.size() < CL_MAX_CHANNELS)
                regions << QnMotionRegion();

            camera->setMotionRegionList(regions, QnDomainMemory);
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
void parseServers(QList<T> &servers, const PbServerList& pb_servers, QnResourceFactory& resourceFactory)
{
    for (PbServerList::const_iterator ci = pb_servers.begin(); ci != pb_servers.end(); ++ci)
    {
        const proto::pb::Server& pb_server = *ci;

        QnVideoServerResourcePtr server(new QnVideoServerResource());
        server->setId(pb_server.id());
        server->setName(QString::fromUtf8(pb_server.name().c_str()));
        server->setUrl(QString::fromUtf8(pb_server.url().c_str()));
        server->setGuid(pb_server.guid().c_str());
        server->setApiUrl(QString::fromUtf8(pb_server.apiurl().c_str()));

        if (pb_server.has_status())
            server->setStatus(static_cast<QnResource::Status>(pb_server.status()));

        if (pb_server.has_netaddrlist())
        {
            QList<QHostAddress> netAddrList;
            deserializeNetAddrList(netAddrList, pb_server.netaddrlist().c_str());
            server->setNetAddrList(netAddrList);
        }

        if (pb_server.has_reserve())
        {
            server->setReserve(pb_server.reserve());
        }

        if (pb_server.storage_size() > 0)
        {
            QnAbstractStorageResourceList storages;

            for (int j = 0; j < pb_server.storage_size(); j++)
            {
                const proto::pb::Server_Storage& pb_storage = pb_server.storage(j);

                //QnAbstractStorageResourcePtr storage(new QnStorageResource());
                QnAbstractStorageResourcePtr storage;

                QnResourceParameters parameters;
                parameters["id"] = QString::number(pb_storage.id());
                parameters["parentId"] = QString::number(pb_server.id());
                parameters["name"] = QString::fromUtf8(pb_storage.name().c_str());
                parameters["url"] = QString::fromUtf8(pb_storage.url().c_str());
                parameters["spaceLimit"] = QString::number(pb_storage.spacelimit());

                QnResourcePtr st = resourceFactory.createResource(qnResTypePool->getResourceTypeByName("Storage")->getId(), parameters);
                storage = qSharedPointerDynamicCast<QnAbstractStorageResource> (st);

                //storage->setId(pb_storage.id());
                //storage->setParentId(pb_server.id());
                //storage->setName(QString::fromUtf8(pb_storage.name().c_str()));

                //storage->setUrl(QString::fromUtf8(pb_storage.url().c_str()));
                //storage->setSpaceLimit(pb_storage.spacelimit());

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

        if (pb_layout.has_guid())
        {
            layout->setGuid(pb_layout.guid().c_str());
            parameters["guid"] = pb_layout.guid().c_str();
        }

        layout->setParentId(pb_layout.parentid());
        parameters["parentId"] = QString::number(pb_layout.parentid());

        layout->setName(QString::fromUtf8(pb_layout.name().c_str()));
        parameters["name"] = QString::fromUtf8(pb_layout.name().c_str());

        layout->setCellAspectRatio(pb_layout.cellaspectratio());
        layout->setCellSpacing(QSizeF(pb_layout.cellspacingwidth(), pb_layout.cellspacingheight()));

        if (pb_layout.item_size() > 0)
        {
            QnLayoutItemDataList items;

            for (int j = 0; j < pb_layout.item_size(); j++)
            {
                const proto::pb::Layout_Item& pb_item = pb_layout.item(j);

                QnLayoutItemData itemData;
                itemData.resource.id = pb_item.resource().id();
                itemData.resource.path = QString::fromUtf8(pb_item.resource().path().c_str());

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

        parameters["name"] = QString::fromUtf8(pb_user.name().c_str());

        user->setName(QString::fromUtf8(pb_user.name().c_str()));
        user->setAdmin(pb_user.isadmin());
		user->setGuid(pb_user.guid().c_str());

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
        resourceType->setName(QString::fromUtf8(pb_resourceType.name().c_str()));
        if (pb_resourceType.has_manufacture())
            resourceType->setManufacture(QString::fromUtf8(pb_resourceType.manufacture().c_str()));

        if (pb_resourceType.parentid_size() > 0)
        {
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
                param->name = QString::fromUtf8(pb_propertyType.name().c_str());
                param->type = (QnParamType::DataType)pb_propertyType.type();

                if (pb_propertyType.has_min())
                    param->min_val = pb_propertyType.min();

                if (pb_propertyType.has_max())
                    param->max_val = pb_propertyType.max();

                if (pb_propertyType.has_step())
                    param->step = pb_propertyType.step();

                if (pb_propertyType.has_values())
                {
                    QString values = QString::fromUtf8(pb_propertyType.values().c_str());
                    foreach(QString val, values.split(QLatin1Char(',')))
                        param->possible_values.push_back(val.trimmed());
                }

                if (pb_propertyType.has_ui_values())
                {
                    QString ui_values = QString::fromUtf8(pb_propertyType.ui_values().c_str());
                    foreach(QString val, ui_values.split(QLatin1Char(',')))
                        param->ui_possible_values.push_back(val.trimmed());
                }

                if (pb_propertyType.has_default_value())
                    param->default_value = QString::fromUtf8(pb_propertyType.default_value().c_str());

                if (pb_propertyType.has_nethelper())
                    param->paramNetHelper = QString::fromUtf8(pb_propertyType.nethelper().c_str());

                if (pb_propertyType.has_group())
                    param->group = QString::fromUtf8(pb_propertyType.group().c_str());

                if (pb_propertyType.has_sub_group())
                    param->subgroup = QString::fromUtf8(pb_propertyType.sub_group().c_str());

                if (pb_propertyType.has_description())
                    param->description = QString::fromUtf8(pb_propertyType.description().c_str());

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

void parseLicenses(QnLicenseList& licenses, const PbLicenseList& pb_licenses)
{
    for (PbLicenseList::const_iterator ci = pb_licenses.begin(); ci != pb_licenses.end(); ++ci)
    {
        const proto::pb::License& pb_license = *ci;

        QnLicensePtr license(new QnLicense(
                                 pb_license.name().c_str(),
                                 pb_license.key().c_str(),
                                 pb_license.cameracount(),
                                 pb_license.hwid().c_str(),
                                 pb_license.signature().c_str()
                                 ));
        licenses.append(license);

    }
}

void parserCameraServerItems(QnCameraHistoryList& cameraServerItems, const PbCameraServerItemList& pb_cameraServerItems)
{
    typedef QMap<qint64, QString> TimestampGuid;
    typedef QMap<QString, TimestampGuid> HistoryType;

    // CameraMAC -> (Timestamp -> ServerGuid)
    HistoryType history;

    // Fill temporary history map
    for (PbCameraServerItemList::const_iterator ci = pb_cameraServerItems.begin(); ci != pb_cameraServerItems.end(); ++ci)
    {
        const proto::pb::CameraServerItem& pb_item = *ci;

        history[pb_item.mac().c_str()][pb_item.timestamp()] = pb_item.serverguid().c_str();
    }

    for(HistoryType::const_iterator ci = history.begin(); ci != history.end(); ++ci)
    {
        QnCameraHistoryPtr cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());

        if (ci.value().isEmpty())
            continue;

        QMapIterator<qint64, QString> camit(ci.value());
        camit.toFront();

        qint64 duration;
        cameraHistory->setMacAddress(ci.key());
        while (camit.hasNext())
        {
            camit.next();

            if (camit.hasNext())
                duration = camit.peekNext().key() - camit.key();
            else
                duration = -1;

            cameraHistory->addTimePeriod(QnCameraTimePeriod(camit.key(), duration, camit.value()));
        }

        cameraServerItems.append(cameraHistory);
    }
}

void serializeCamera_i(proto::pb::Camera& pb_camera, const QnVirtualCameraResourcePtr& cameraPtr)
{
    pb_camera.set_id(cameraPtr->getId().toInt());
    pb_camera.set_parentid(cameraPtr->getParentId().toInt());
    pb_camera.set_name(cameraPtr->getName().toUtf8().constData());
    pb_camera.set_typeid_(cameraPtr->getTypeId().toInt());
    pb_camera.set_url(cameraPtr->getUrl().toUtf8().constData());
    pb_camera.set_mac(cameraPtr->getMAC().toString().toUtf8().constData());
    pb_camera.set_login(cameraPtr->getAuth().user().toUtf8().constData());
    pb_camera.set_password(cameraPtr->getAuth().password().toUtf8().constData());
	pb_camera.set_disabled(cameraPtr->isDisabled());
    pb_camera.set_status(static_cast<proto::pb::Camera_Status>(cameraPtr->getStatus()));
    pb_camera.set_region(serializeMotionRegionList(cameraPtr->getMotionRegionList()).toUtf8().constData());
    pb_camera.set_scheduledisabled(cameraPtr->isScheduleDisabled());
    pb_camera.set_motiontype(static_cast<proto::pb::Camera_MotionType>(cameraPtr->getMotionType()));

    foreach(const QnScheduleTask& scheduleTaskIn, cameraPtr->getScheduleTasks()) {
        proto::pb::Camera_ScheduleTask& pb_scheduleTask = *pb_camera.add_scheduletask();

        pb_scheduleTask.set_id(scheduleTaskIn.getId().toInt());
        pb_scheduleTask.set_sourceid(cameraPtr->getId().toInt());
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
}

void serializeCameraServerItem_i(proto::pb::CameraServerItem& pb_cameraServerItem, const QnCameraHistoryItem& cameraServerItem)
{
    pb_cameraServerItem.set_mac(cameraServerItem.mac.toStdString());
    pb_cameraServerItem.set_timestamp(cameraServerItem.timestamp);
    pb_cameraServerItem.set_serverguid(cameraServerItem.videoServerGuid.toStdString());
}

void serializeLayout_i(proto::pb::Layout& pb_layout, const QnLayoutResourcePtr& layoutIn)
{
    pb_layout.set_parentid(layoutIn->getParentId().toInt());
    pb_layout.set_name(layoutIn->getName().toUtf8().constData());
    pb_layout.set_guid(layoutIn->getGuid().toAscii().constData());

    pb_layout.set_cellaspectratio(layoutIn->cellAspectRatio());
    pb_layout.set_cellspacingwidth(layoutIn->cellSpacing().width());
    pb_layout.set_cellspacingheight(layoutIn->cellSpacing().height());

    if (!layoutIn->getItems().isEmpty()) {
        foreach(const QnLayoutItemData& itemIn, layoutIn->getItems()) {
            proto::pb::Layout_Item& pb_item = *pb_layout.add_item();

            pb_item.mutable_resource()->set_path(itemIn.resource.path.toUtf8().constData());
            if (itemIn.resource.id.isValid() && !itemIn.resource.id.isSpecial())
                pb_item.mutable_resource()->set_id(itemIn.resource.id.toInt());

            pb_item.set_uuid(itemIn.uuid.toString().toUtf8().constData());
            pb_item.set_flags(itemIn.flags);
            pb_item.set_left(itemIn.combinedGeometry.left());
            pb_item.set_top(itemIn.combinedGeometry.top());
            pb_item.set_right(itemIn.combinedGeometry.right());
            pb_item.set_bottom(itemIn.combinedGeometry.bottom());
            pb_item.set_rotation(itemIn.rotation);
        }
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

void QnApiPbSerializer::deserializeServers(QnVideoServerResourceList& servers, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    proto::pb::Servers pb_servers;
    if (!pb_servers.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeServers(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseServers(servers, pb_servers.server(), resourceFactory);
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

void QnApiPbSerializer::deserializeLayout(QnLayoutResourcePtr& layout, const QByteArray& data)
{
    QnLayoutResourceList layouts;
    deserializeLayouts(layouts, data);

    if (layouts.isEmpty())
        layout = QnLayoutResourcePtr();
    else
        layout = layouts.at(0); 
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
    parseServers(resources, pb_resources.server(), resourceFactory);
	parseUsers(resources, pb_resources.user());
	parseLayouts(resources, pb_resources.layout());
}

void QnApiPbSerializer::deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data)
{
    proto::pb::ResourceTypes pb_resourceTypes;
    if (!pb_resourceTypes.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeResourceTypes(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseResourceTypes(resourceTypes, pb_resourceTypes.resourcetype());
}

void QnApiPbSerializer::deserializeLicenses(QnLicenseList &licenses, const QByteArray &data)
{
    proto::pb::Licenses pb_licenses;
    if (!pb_licenses.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeLicenses(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    licenses.setHardwareId(pb_licenses.hwid().c_str());
    parseLicenses(licenses, pb_licenses.license());
}

void QnApiPbSerializer::deserializeCameraHistoryList(QnCameraHistoryList &cameraHistoryList, const QByteArray &data)
{
    proto::pb::CameraServerItems pb_csis;

    if (!pb_csis.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeLicenses(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parserCameraServerItems(cameraHistoryList, pb_csis.cameraserveritem());
}

void QnApiPbSerializer::serializeCameras(const QnVirtualCameraResourceList& cameras, QByteArray& data)
{
    proto::pb::Cameras pb_cameras;

    foreach(QnVirtualCameraResourcePtr cameraPtr, cameras)
        serializeCamera_i(*pb_cameras.add_camera(), cameraPtr);

    std::string str;
    pb_cameras.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeLicense(const QnLicensePtr& license, QByteArray& data)
{
    proto::pb::Licenses pb_licenses;

    proto::pb::License& pb_license = *(pb_licenses.add_license());
    pb_license.set_name(license->name().constData());
    pb_license.set_key(license->key().constData());
    pb_license.set_cameracount(license->cameraCount());
    pb_license.set_hwid(license->hardwareId().constData());
    pb_license.set_signature(license->signature().constData());

    std::string str;
    pb_licenses.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeServer(const QnVideoServerResourcePtr& serverPtr, QByteArray& data)
{
    proto::pb::Servers pb_servers;

    proto::pb::Server& pb_server = *pb_servers.add_server();

    pb_server.set_id(serverPtr->getId().toInt());
    pb_server.set_name(serverPtr->getName().toUtf8().constData());
    pb_server.set_url(serverPtr->getUrl().toUtf8().constData());
    pb_server.set_guid(serverPtr->getGuid().toAscii().constData());
    pb_server.set_apiurl(serverPtr->getApiUrl().toUtf8().constData());

    if (!serverPtr->getNetAddrList().isEmpty())
        pb_server.set_netaddrlist(serializeNetAddrList(serverPtr->getNetAddrList()).toUtf8().constData());

    pb_server.set_reserve(serverPtr->getReserve());

    if (!serverPtr->getStorages().isEmpty()) {
        foreach (const QnAbstractStorageResourcePtr& storagePtr, serverPtr->getStorages()) {
            proto::pb::Server_Storage& pb_storage = *pb_server.add_storage();

            pb_storage.set_id(storagePtr->getId().toInt());
            pb_storage.set_name(storagePtr->getName().toUtf8().constData());
            pb_storage.set_url(storagePtr->getUrl().toUtf8().constData());
            pb_storage.set_spacelimit(storagePtr->getSpaceLimit());
        }
    }

    std::string str;
    pb_servers.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeUser(const QnUserResourcePtr& userPtr, QByteArray& data)
{
    proto::pb::Users pb_users;
    proto::pb::User& pb_user = *pb_users.add_user();

    if (userPtr->getId().isValid())
        pb_user.set_id(userPtr->getId().toInt());

    pb_user.set_name(userPtr->getName().toUtf8().constData());
    pb_user.set_password(userPtr->getPassword().toUtf8().constData());
    pb_user.set_isadmin(userPtr->isAdmin());
	pb_user.set_guid(userPtr->getGuid().toAscii().constData());

    std::string str;
    pb_users.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeCamera(const QnVirtualCameraResourcePtr& cameraPtr, QByteArray& data)
{
    proto::pb::Cameras pb_cameras;
    serializeCamera_i(*pb_cameras.add_camera(), cameraPtr);

    std::string str;
    pb_cameras.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeLayout(const QnLayoutResourcePtr& layout, QByteArray& data)
{
    proto::pb::Layouts pb_layouts;
    serializeLayout_i(*pb_layouts.add_layout(), layout);

    std::string str;
    pb_layouts.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data)
{
    proto::pb::Layouts pb_layouts;

    foreach(QnLayoutResourcePtr layout, layouts)
        serializeLayout_i(*pb_layouts.add_layout(), layout);

    std::string str;
    pb_layouts.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeCameraServerItem(const QnCameraHistoryItem& cameraServerItem, QByteArray &data)
{
    proto::pb::CameraServerItems pb_cameraServerItems;
    serializeCameraServerItem_i(*pb_cameraServerItems.add_cameraserveritem(), cameraServerItem);

    std::string str;
    pb_cameraServerItems.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

