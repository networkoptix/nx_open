#include "camera.pb.h"
#include "server.pb.h"
#include "user.pb.h"
#include "layout.pb.h"
#include "resource.pb.h"
#include "resourceType.pb.h"
#include "license.pb.h"
#include "cameraServerItem.pb.h"
#include "connectinfo.pb.h"

#include "pb_serializer.h"

void parseCameraServerItem(QnCameraHistoryItemPtr& historyItem, const pb::CameraServerItem& pb_cameraServerItem);
void parseLicense(QnLicensePtr& license, const pb::License& pb_license);
void parseResource(QnResourcePtr& resource, const pb::Resource& pb_resource, QnResourceFactory& resourceFactory);

namespace {

typedef google::protobuf::RepeatedPtrField<pb::Resource>           PbResourceList;
typedef google::protobuf::RepeatedPtrField<pb::ResourceType>     PbResourceTypeList;
typedef google::protobuf::RepeatedPtrField<pb::License>          PbLicenseList;
typedef google::protobuf::RepeatedPtrField<pb::CameraServerItem> PbCameraServerItemList;

QString serializeNetAddrList(const QList<QHostAddress>& netAddrList)
{
    QStringList addListStrings;
    std::transform(netAddrList.begin(), netAddrList.end(), std::back_inserter(addListStrings), std::mem_fun_ref(&QHostAddress::toString));
    return addListStrings.join(QLatin1String(";"));
}

QHostAddress stringToAddr(const QString& hostStr)
{
    return QHostAddress(hostStr);
}

void deserializeNetAddrList(QList<QHostAddress>& netAddrList, const QString& netAddrListString)
{
    QStringList addListStrings = netAddrListString.split(QLatin1Char(';'));
    std::transform(addListStrings.begin(), addListStrings.end(), std::back_inserter(netAddrList), stringToAddr);
}

void parseCamera(QnVirtualCameraResourcePtr& camera, const pb::Resource& pb_cameraResource, QnResourceFactory& resourceFactory)
{
    const pb::Camera& pb_camera = pb_cameraResource.GetExtension(pb::Camera::resource);

    QnResourceParameters parameters;
    parameters["id"] = QString::number(pb_cameraResource.id());
    parameters["name"] = QString::fromUtf8(pb_cameraResource.name().c_str());
    parameters["url"] = QString::fromUtf8(pb_cameraResource.url().c_str());
    parameters["mac"] = QString::fromUtf8(pb_camera.mac().c_str());
    parameters["physicalId"] = QString::fromUtf8(pb_camera.physicalid().c_str());
    parameters["login"] = QString::fromUtf8(pb_camera.login().c_str());
    parameters["password"] = QString::fromUtf8(pb_camera.password().c_str());

    if (pb_cameraResource.has_status())
        parameters["status"] = QString::number((int)pb_cameraResource.status());

    parameters["disabled"] = QString::number((int)pb_cameraResource.disabled());
    parameters["parentId"] = QString::number(pb_cameraResource.parentid());

    QnResourcePtr cameraBase = resourceFactory.createResource(pb_cameraResource.typeid_(), parameters);
    if (cameraBase.isNull())
        return;

    camera = cameraBase.dynamicCast<QnVirtualCameraResource>();
    if (camera.isNull())
        return;

    for (int j = 0; j < pb_cameraResource.property_size(); j++)
    {
        const pb::Resource_Property& pb_property = pb_cameraResource.property(j);

        camera->setParam(QString::fromStdString(pb_property.name()), QString::fromStdString(pb_property.value()), QnDomainDatabase);
    }

    camera->setScheduleDisabled(pb_camera.scheduledisabled());
    if (pb_camera.has_audioenabled())
        camera->setAudioEnabled(pb_camera.audioenabled());

    if (pb_camera.has_physicalid())
        camera->setPhysicalId(QString::fromStdString(pb_camera.physicalid()));

    camera->setAuth(QString::fromUtf8(pb_camera.login().c_str()), QString::fromUtf8(pb_camera.password().c_str()));
    camera->setMotionType(static_cast<MotionType>(pb_camera.motiontype()));

    if (pb_camera.has_region())
    {
        QList<QnMotionRegion> regions;
        parseMotionRegionList(regions, QString::fromStdString(pb_camera.region()));
        while (regions.size() < CL_MAX_CHANNELS)
            regions << QnMotionRegion();

        camera->setMotionRegionList(regions, QnDomainMemory);
    }

    if (pb_camera.scheduletask_size())
    {
        QnScheduleTaskList scheduleTasks;

        for (int j = 0; j < pb_camera.scheduletask_size(); j++)
        {
            const pb::Camera_ScheduleTask& pb_scheduleTask = pb_camera.scheduletask(j);

            QnScheduleTask scheduleTask(pb_scheduleTask.id(),
                                        pb_cameraResource.id(),
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
}

void parseCameras(QnVirtualCameraResourceList& cameras, const PbResourceList& pb_cameras, QnResourceFactory& resourceFactory)
{
    for (PbResourceList::const_iterator ci = pb_cameras.begin(); ci != pb_cameras.end(); ++ci)
    {
        const pb::Resource& pb_cameraResource = *ci;

        QnVirtualCameraResourcePtr camera;
        parseCamera(camera, pb_cameraResource, resourceFactory);
        if (camera)
            cameras.append(camera);
        else
            cl_log.log("Can't create resource with id=", ci->id(), cl_logWARNING);
    }
}

void parseServer(QnVideoServerResourcePtr &server, const pb::Resource &pb_serverResource, QnResourceFactory &resourceFactory)
{
    const pb::Server& pb_server = pb_serverResource.GetExtension(pb::Server::resource);

    server = QnVideoServerResourcePtr(new QnVideoServerResource());
    server->setId(pb_serverResource.id());
    server->setName(QString::fromUtf8(pb_serverResource.name().c_str()));
    server->setUrl(QString::fromUtf8(pb_serverResource.url().c_str()));
    server->setGuid(QString::fromStdString(pb_serverResource.guid()));
    server->setApiUrl(QString::fromUtf8(pb_server.apiurl().c_str()));
    if (pb_server.has_streamingurl())
        server->setStreamingUrl(QString::fromUtf8(pb_server.streamingurl().c_str()));

    if (pb_serverResource.has_status())
        server->setStatus(static_cast<QnResource::Status>(pb_serverResource.status()));

    if (pb_server.has_netaddrlist())
    {
        QList<QHostAddress> netAddrList;
        //TODO:UTF unuse std::string
        deserializeNetAddrList(netAddrList, QString::fromStdString(pb_server.netaddrlist()));
        server->setNetAddrList(netAddrList);
    }

    if (pb_server.has_reserve())
    {
        server->setReserve(pb_server.reserve());
    }

    if (pb_server.has_panicmode())
    {
        server->setPanicMode(pb_server.panicmode());
    }

    if (pb_server.storage_size() > 0)
    {
        QnAbstractStorageResourceList storages;

        for (int j = 0; j < pb_server.storage_size(); j++)
        {
            const pb::Server_Storage& pb_storage = pb_server.storage(j);

            //QnAbstractStorageResourcePtr storage(new QnStorageResource());
            QnAbstractStorageResourcePtr storage;

            QnResourceParameters parameters;
            parameters["id"] = QString::number(pb_storage.id());
            parameters["parentId"] = QString::number(pb_serverResource.id());
            parameters["name"] = QString::fromUtf8(pb_storage.name().c_str());
            parameters["url"] = QString::fromUtf8(pb_storage.url().c_str());
            parameters["spaceLimit"] = QString::number(pb_storage.spacelimit());

            QnResourcePtr st = resourceFactory.createResource(qnResTypePool->getResourceTypeByName(QLatin1String("Storage"))->getId(), parameters);
            storage = qSharedPointerDynamicCast<QnAbstractStorageResource> (st);

            storages.append(storage);
        }

        server->setStorages(storages);
    }
}

void parseServers(QnVideoServerResourceList &servers, const PbResourceList &pb_servers, QnResourceFactory &resourceFactory)
{
    for (PbResourceList::const_iterator ci = pb_servers.begin(); ci != pb_servers.end(); ++ci)
    {
        const pb::Resource& pb_serverResource = *ci;

        QnVideoServerResourcePtr server;
        parseServer(server, pb_serverResource, resourceFactory);
        if (server)
            servers.append(server);
        else
            cl_log.log("Can't create resource with id=", ci->id(), cl_logWARNING);
    }
}

void parseLayout(QnLayoutResourcePtr& layout, const pb::Resource& pb_layoutResource)
{
    const pb::Layout& pb_layout = pb_layoutResource.GetExtension(pb::Layout::resource);

    layout = QnLayoutResourcePtr(new QnLayoutResource());

    if (pb_layoutResource.has_id())
        layout->setId(pb_layoutResource.id());

    if (pb_layoutResource.has_guid())
        layout->setGuid(QString::fromStdString(pb_layoutResource.guid()));

    layout->setParentId(pb_layoutResource.parentid());
    layout->setName(QString::fromUtf8(pb_layoutResource.name().c_str()));
    layout->setCellAspectRatio(pb_layout.cellaspectratio());
    layout->setCellSpacing(QSizeF(pb_layout.cellspacingwidth(), pb_layout.cellspacingheight()));

    if (pb_layout.item_size() > 0)
    {
        QnLayoutItemDataList items;

        for (int j = 0; j < pb_layout.item_size(); j++)
        {
            const pb::Layout_Item& pb_item = pb_layout.item(j);

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
}

void parseLayouts(QnLayoutResourceList& layouts, const PbResourceList& pb_layouts)
{
    for (PbResourceList::const_iterator ci = pb_layouts.begin(); ci != pb_layouts.end(); ++ci)
    {
        const pb::Resource& pb_layoutResource = *ci;
        QnLayoutResourcePtr layout;
        parseLayout(layout, pb_layoutResource);
        if (layout)
            layouts.append(layout);
        else
            cl_log.log("Can't create resource with id=", ci->id(), cl_logWARNING);
    }
}

void parseUser(QnUserResourcePtr& user, const pb::Resource& pb_userResource)
{
    const pb::User& pb_user = pb_userResource.GetExtension(pb::User::resource);

    user = QnUserResourcePtr(new QnUserResource());

    if (pb_userResource.has_id())
        user->setId(pb_userResource.id());

    user->setName(QString::fromUtf8(pb_userResource.name().c_str()));
    user->setAdmin(pb_user.isadmin());
    user->setPermissions(pb_user.rights());
    user->setGuid(QString::fromUtf8(pb_userResource.guid().c_str()));
}

void parseUsers(QnUserResourceList& users, const PbResourceList& pb_users)
{
    for (PbResourceList::const_iterator ci = pb_users.begin(); ci != pb_users.end(); ++ci)
    {
        const pb::Resource& pb_userResource = *ci;
        QnUserResourcePtr user;
        parseUser(user, pb_userResource);
        if (user)
            users.append(user);
        else
            cl_log.log("Can't create resource with id=", ci->id(), cl_logWARNING);
    }
}

void parseResources(QnResourceList& resources, const PbResourceList& pb_resources, QnResourceFactory& resourceFactory)
{
    for (PbResourceList::const_iterator ci = pb_resources.begin(); ci != pb_resources.end(); ++ci)
    {
        const pb::Resource& pb_resource = *ci;

        QnResourcePtr resource;
        parseResource(resource, pb_resource, resourceFactory);
        if (resource)
            resources.append(resource);
        else
            cl_log.log("Can't create resource with id=", ci->id(), cl_logWARNING);
    }
}

void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const PbResourceTypeList& pb_resourceTypes)
{
    for (PbResourceTypeList::const_iterator ci = pb_resourceTypes.begin (); ci != pb_resourceTypes.end (); ++ci)
    {
        const pb::ResourceType& pb_resourceType = *ci;

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
                const pb::PropertyType& pb_propertyType = pb_resourceType.propertytype(i);

                QnParamTypePtr param(new QnParamType());
                param->name = QString::fromUtf8(pb_propertyType.name().c_str());
                param->type = (QnParamType::DataType)pb_propertyType.type();

                param->id = pb_propertyType.id();

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
        const pb::License& pb_license = *ci;

        QnLicensePtr license;
        parseLicense(license, pb_license);
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
        const pb::CameraServerItem& pb_item = *ci;
        //TODO:UTF unuse std::string
        history[QString::fromStdString(pb_item.physicalid())][pb_item.timestamp()] = QString::fromStdString(pb_item.serverguid());
    }

    for(HistoryType::const_iterator ci = history.begin(); ci != history.end(); ++ci)
    {
        QnCameraHistoryPtr cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());

        if (ci.value().isEmpty())
            continue;

        QMapIterator<qint64, QString> camit(ci.value());
        camit.toFront();

        qint64 duration;
        cameraHistory->setPhysicalId(ci.key());
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

void serializeCamera_i(pb::Resource& pb_cameraResource, const QnVirtualCameraResourcePtr& cameraPtr)
{
    pb_cameraResource.set_type(pb::Resource_Type_Camera);
    pb::Camera &pb_camera = *pb_cameraResource.MutableExtension(pb::Camera::resource);

    pb_cameraResource.set_id(cameraPtr->getId().toInt());
    pb_cameraResource.set_parentid(cameraPtr->getParentId().toInt());
    pb_cameraResource.set_name(cameraPtr->getName().toUtf8().constData());
    pb_cameraResource.set_typeid_(cameraPtr->getTypeId().toInt());
    pb_cameraResource.set_url(cameraPtr->getUrl().toUtf8().constData());
    pb_camera.set_mac(cameraPtr->getMAC().toString().toUtf8().constData());
    pb_camera.set_physicalid(cameraPtr->getPhysicalId().toUtf8().constData());
    pb_camera.set_login(cameraPtr->getAuth().user().toUtf8().constData());
    pb_camera.set_password(cameraPtr->getAuth().password().toUtf8().constData());
    pb_cameraResource.set_disabled(cameraPtr->isDisabled());
    pb_cameraResource.set_status(static_cast<pb::Resource_Status>(cameraPtr->getStatus()));
    pb_camera.set_region(serializeMotionRegionList(cameraPtr->getMotionRegionList()).toUtf8().constData());
    pb_camera.set_scheduledisabled(cameraPtr->isScheduleDisabled());
    pb_camera.set_audioenabled(cameraPtr->isAudioEnabled());
    pb_camera.set_motiontype(static_cast<pb::Camera_MotionType>(cameraPtr->getMotionType()));

    QnParamList params = cameraPtr->getResourceParamList();
    foreach(QString key, params.keys())
    {
        if (params[key].domain() == QnDomainDatabase)
        {
            pb::Resource_Property& pb_property = *pb_cameraResource.add_property();

            pb_property.set_id(params[key].paramTypeId().toInt());
            pb_property.set_name(key.toUtf8().constData());
            pb_property.set_value(params[key].value().toString().toUtf8().constData());
        }
    }

    foreach(const QnScheduleTask& scheduleTaskIn, cameraPtr->getScheduleTasks())
    {
        pb::Camera_ScheduleTask& pb_scheduleTask = *pb_camera.add_scheduletask();

        pb_scheduleTask.set_id(scheduleTaskIn.getId().toInt());
        pb_scheduleTask.set_sourceid(cameraPtr->getId().toInt());
        pb_scheduleTask.set_starttime(scheduleTaskIn.getStartTime());
        pb_scheduleTask.set_endtime(scheduleTaskIn.getEndTime());
        pb_scheduleTask.set_dorecordaudio(scheduleTaskIn.getDoRecordAudio());
        pb_scheduleTask.set_recordtype(scheduleTaskIn.getRecordingType());
        pb_scheduleTask.set_dayofweek(scheduleTaskIn.getDayOfWeek());
        pb_scheduleTask.set_beforethreshold(scheduleTaskIn.getBeforeThreshold());
        pb_scheduleTask.set_afterthreshold(scheduleTaskIn.getAfterThreshold());
        pb_scheduleTask.set_streamquality((pb::Camera_Quality)scheduleTaskIn.getStreamQuality());
        pb_scheduleTask.set_fps(scheduleTaskIn.getFps());
    }
}

void serializeCameraServerItem_i(pb::CameraServerItem& pb_cameraServerItem, const QnCameraHistoryItem& cameraServerItem)
{
    pb_cameraServerItem.set_physicalid(cameraServerItem.physicalId.toStdString());
    pb_cameraServerItem.set_timestamp(cameraServerItem.timestamp);
    pb_cameraServerItem.set_serverguid(cameraServerItem.videoServerGuid.toStdString());
}

void serializeLayout_i(pb::Resource& pb_layoutResource, const QnLayoutResourcePtr& layoutIn)
{
    pb_layoutResource.set_type(pb::Resource_Type_Layout);
    pb::Layout &pb_layout = *pb_layoutResource.MutableExtension(pb::Layout::resource);

    pb_layoutResource.set_parentid(layoutIn->getParentId().toInt());
    pb_layoutResource.set_name(layoutIn->getName().toUtf8().constData());
    pb_layoutResource.set_guid(layoutIn->getGuid().toAscii().constData());

    pb_layout.set_cellaspectratio(layoutIn->cellAspectRatio());
    pb_layout.set_cellspacingwidth(layoutIn->cellSpacing().width());
    pb_layout.set_cellspacingheight(layoutIn->cellSpacing().height());

    if (!layoutIn->getItems().isEmpty()) {
        foreach(const QnLayoutItemData& itemIn, layoutIn->getItems()) {
            pb::Layout_Item& pb_item = *pb_layout.add_item();

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
    pb::Resources pb_cameras;
    if (!pb_cameras.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeCameras(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseCameras(cameras, pb_cameras.resource(), resourceFactory);
}

void QnApiPbSerializer::deserializeServers(QnVideoServerResourceList& servers, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    pb::Resources pb_servers;
    if (!pb_servers.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeServers(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseServers(servers, pb_servers.resource(), resourceFactory);
}

void QnApiPbSerializer::deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data)
{
    pb::Resources pb_layouts;
    if (!pb_layouts.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeLayouts(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseLayouts(layouts, pb_layouts.resource());
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
    pb::Resources pb_users;
    if (!pb_users.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeUsers(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseUsers(users, pb_users.resource());
}

void QnApiPbSerializer::deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    pb::Resources pb_resources;
    if (!pb_resources.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeCameras(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseResources(resources, pb_resources.resource(), resourceFactory);
}

void QnApiPbSerializer::deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data)
{
    pb::ResourceTypes pb_resourceTypes;
    if (!pb_resourceTypes.ParseFromArray(data.data(), data.size())) {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeResourceTypes(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parseResourceTypes(resourceTypes, pb_resourceTypes.resourcetype());
}

void QnApiPbSerializer::deserializeLicenses(QnLicenseList &licenses, const QByteArray &data)
{
    pb::Licenses pb_licenses;
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
    pb::CameraServerItems pb_csis;

    if (!pb_csis.ParseFromArray(data.data(), data.size()))
    {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeLicenses(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    parserCameraServerItems(cameraHistoryList, pb_csis.cameraserveritem());
}

void QnApiPbSerializer::deserializeConnectInfo(QnConnectInfoPtr& connectInfo, const QByteArray& data)
{
    pb::ConnectInfo pb_connectInfo;

    if (!pb_connectInfo.ParseFromArray(data.data(), data.size()))
    {
        QByteArray errorString;
        errorString = "QnApiPbSerializer::deserializeConnectInfo(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    //TODO:UTF unuse std::string
    connectInfo->version = QString::fromStdString(pb_connectInfo.version());

    typedef google::protobuf::RepeatedPtrField<pb::CompatibilityItem> PbCompatibilityItemList;
    PbCompatibilityItemList items = pb_connectInfo.compatibilityitems().item();

    for (PbCompatibilityItemList::const_iterator ci = items.begin(); ci != items.end(); ++ci)
    {
        //TODO:UTF unuse std::string
        connectInfo->compatibilityItems.append(QnCompatibilityItem(QString::fromStdString(ci->ver1()), 
            QString::fromStdString(ci->comp1()), QString::fromStdString(ci->ver2())));
    }
    connectInfo->proxyPort = pb_connectInfo.proxyport();
}

void QnApiPbSerializer::serializeCameras(const QnVirtualCameraResourceList& cameras, QByteArray& data)
{
    pb::Resources pb_cameras;

    foreach(QnVirtualCameraResourcePtr cameraPtr, cameras)
        serializeCamera_i(*pb_cameras.add_resource(), cameraPtr);

    std::string str;
    pb_cameras.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeLicense(const QnLicensePtr& license, QByteArray& data)
{
    pb::Licenses pb_licenses;

    pb::License& pb_license = *(pb_licenses.add_license());
    pb_license.set_name(license->name().toUtf8().constData());
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
    pb::Resources pb_servers;

    pb::Resource& pb_serverResource = *pb_servers.add_resource();
    pb_serverResource.set_type(pb::Resource_Type_Server);
    pb::Server &pb_server = *pb_serverResource.MutableExtension(pb::Server::resource);

    pb_serverResource.set_id(serverPtr->getId().toInt());
    pb_serverResource.set_name(serverPtr->getName().toUtf8().constData());
    pb_serverResource.set_url(serverPtr->getUrl().toUtf8().constData());
    pb_serverResource.set_guid(serverPtr->getGuid().toAscii().constData());
    pb_server.set_apiurl(serverPtr->getApiUrl().toUtf8().constData());
    pb_server.set_streamingurl(serverPtr->getStreamingUrl().toUtf8().constData());
    pb_serverResource.set_status(static_cast<pb::Resource_Status>(serverPtr->getStatus()));

    if (!serverPtr->getNetAddrList().isEmpty())
        pb_server.set_netaddrlist(serializeNetAddrList(serverPtr->getNetAddrList()).toUtf8().constData());

    pb_server.set_reserve(serverPtr->getReserve());
    pb_server.set_panicmode(serverPtr->isPanicMode());

    if (!serverPtr->getStorages().isEmpty()) {
        foreach (const QnAbstractStorageResourcePtr& storagePtr, serverPtr->getStorages()) {
            pb::Server_Storage& pb_storage = *pb_server.add_storage();

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
    pb::Resources pb_users;
    pb::Resource& pb_userResource = *pb_users.add_resource();

    pb_userResource.set_type(pb::Resource_Type_User);
    pb::User &pb_user = *pb_userResource.MutableExtension(pb::User::resource);

    if (userPtr->getId().isValid())
        pb_userResource.set_id(userPtr->getId().toInt());

    pb_userResource.set_name(userPtr->getName().toUtf8().constData());
    pb_user.set_password(userPtr->getPassword().toUtf8().constData());
    pb_user.set_rights(userPtr->getPermissions());
    pb_user.set_isadmin(userPtr->isAdmin());
    pb_userResource.set_guid(userPtr->getGuid().toUtf8().constData());

    std::string str;
    pb_users.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeCamera(const QnVirtualCameraResourcePtr& cameraPtr, QByteArray& data)
{
    pb::Resources pb_cameras;
    serializeCamera_i(*pb_cameras.add_resource(), cameraPtr);

    std::string str;
    pb_cameras.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeLayout(const QnLayoutResourcePtr& layout, QByteArray& data)
{
    pb::Resources pb_layouts;
    serializeLayout_i(*pb_layouts.add_resource(), layout);

    std::string str;
    pb_layouts.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data)
{
    pb::Resources pb_layouts;

    foreach(QnLayoutResourcePtr layout, layouts)
        serializeLayout_i(*pb_layouts.add_resource(), layout);

    std::string str;
    pb_layouts.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void QnApiPbSerializer::serializeCameraServerItem(const QnCameraHistoryItem& cameraServerItem, QByteArray &data)
{
    pb::CameraServerItems pb_cameraServerItems;
    serializeCameraServerItem_i(*pb_cameraServerItems.add_cameraserveritem(), cameraServerItem);

    std::string str;
    pb_cameraServerItems.SerializeToString(&str);
    data = QByteArray(str.data(), str.length());
}

void parseResource(QnResourcePtr& resource, const pb::Resource& pb_resource, QnResourceFactory& resourceFactory)
{
    switch (pb_resource.type())
    {
        case pb::Resource_Type_Camera:
        {
            QnVirtualCameraResourcePtr camera;
            parseCamera(camera, pb_resource, resourceFactory);
            resource = camera;
            break;
        }
        case pb::Resource_Type_Server:
        {
            QnVideoServerResourcePtr server;
            parseServer(server, pb_resource, resourceFactory);
            resource = server;
            break;
        }
        case pb::Resource_Type_User:
        {
            QnUserResourcePtr user;
            parseUser(user, pb_resource);
            resource = user;
            break;
        }
        case pb::Resource_Type_Layout:
        {
            QnLayoutResourcePtr layout;
            parseLayout(layout, pb_resource);
            resource = layout;
            break;
        }
    }
}

void parseLicense(QnLicensePtr& license, const pb::License& pb_license)
{
    license = QnLicensePtr(new QnLicense(
                            QString::fromUtf8(pb_license.name().c_str()),
                            pb_license.key().c_str(),
                            pb_license.cameracount(),
                            pb_license.hwid().c_str(),
                            pb_license.signature().c_str()
                            ));
}

void parseCameraServerItem(QnCameraHistoryItemPtr& historyItem, const pb::CameraServerItem& pb_cameraServerItem)
{
    //TODO:UTF unuse std::string
    historyItem = QnCameraHistoryItemPtr(new QnCameraHistoryItem(
                                            QString::fromStdString(pb_cameraServerItem.physicalid()),
                                            pb_cameraServerItem.timestamp(),
                                            QString::fromStdString(pb_cameraServerItem.serverguid())
                                        ));

}

