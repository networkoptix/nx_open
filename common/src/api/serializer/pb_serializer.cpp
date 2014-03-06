#include "version.h"

#undef PlaySound // TODO: #Elric wtf??????

#include "camera.pb.h"
#include "server.pb.h"
#include "user.pb.h"
#include "layout.pb.h"
#include "resource.pb.h"
#include "resourceType.pb.h"
#include "license.pb.h"
#include "cameraServerItem.pb.h"
#include "connectinfo.pb.h"
#include "businessRule.pb.h"
#include "email.pb.h"
#include "kvpair.pb.h"
#include "setting.pb.h"

#include "pb_serializer.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/item_dewarping_params.h>

#include <business/business_action_factory.h>

#include <utils/common/json.h>

#include <QtSql/QSqlRecord>

/* Prohibit the usage of std::string-QString conversion functions that do not 
 * explicitly state the encoding used for conversion. 
 * 
 * If you intend to comment one of these lines out, think twice.
 * My sword is sharp. */
#define fromStdString use_fromUtf8_or_fromLatin1
#define toStdString use_toUtf8_or_toLatin1

namespace {

typedef google::protobuf::RepeatedPtrField<pb::Resource>            PbResourceList;
typedef google::protobuf::RepeatedPtrField<pb::ResourceType>        PbResourceTypeList;
typedef google::protobuf::RepeatedPtrField<pb::License>             PbLicenseList;
typedef google::protobuf::RepeatedPtrField<pb::CameraServerItem>    PbCameraServerItemList;
typedef google::protobuf::RepeatedPtrField<pb::BusinessRule>        PbBusinessRuleList;
typedef google::protobuf::RepeatedPtrField<pb::KvPair>              PbKvPairList;
typedef google::protobuf::RepeatedPtrField<pb::Setting>             PbSettingList;
}

void parseCameraServerItem(QnCameraHistoryItemPtr& historyItem, const pb::CameraServerItem& pb_cameraServerItem);
void parseLicense(QnLicensePtr& license, const pb::License& pb_license);
void parseResource(QnResourcePtr& resource, const pb::Resource& pb_resource, QnResourceFactory& resourceFactory);
void parseBusinessRule(QnBusinessEventRulePtr& businessRule, const pb::BusinessRule& pb_businessRule);
void parseBusinessRules(QnBusinessEventRuleList& businessRules, const PbBusinessRuleList& pb_businessRules);
void parseBusinessAction(QnAbstractBusinessActionPtr& businessAction, const pb::BusinessAction& pb_businessAction);
void parseBusinessActionVector(QnBusinessActionDataListPtr& businessActionList, const pb::BusinessActionList& pb_businessActionList);
void parseResources(QnResourceList& resources, const PbResourceList& pb_resources, QnResourceFactory& resourceFactory);
void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const PbResourceTypeList& pb_resourceTypes);
void parseLicenses(QnLicenseList& licenses, const PbLicenseList& pb_licenses);
void parseCameraServerItems(QnCameraHistoryList& cameraServerItems, const PbCameraServerItemList& pb_cameraServerItems);
void parseKvPairs(QnKvPairListsById& kvPairs, const PbKvPairList& pb_kvPairs);

namespace {

QString serializeNetAddrList(const QList<QHostAddress>& netAddrList)
{
    QStringList addListStrings;
    std::transform(netAddrList.begin(), netAddrList.end(), std::back_inserter(addListStrings), std::mem_fun_ref(&QHostAddress::toString));
    return addListStrings.join(QLatin1String(";"));
}

static QHostAddress stringToAddr(const QString& hostStr)
{
    return QHostAddress(hostStr);
}

void deserializeNetAddrList(QList<QHostAddress>& netAddrList, const QString& netAddrListString)
{
    QStringList addListStrings = netAddrListString.split(QLatin1Char(';'));
    std::transform(addListStrings.begin(), addListStrings.end(), std::back_inserter(netAddrList), stringToAddr);
}

void parseCamera(QnNetworkResourcePtr& camera, const pb::Resource& pb_cameraResource, QnResourceFactory& resourceFactory)
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

    camera = resourceFactory.createResource(pb_cameraResource.resourcetypeid(), parameters).dynamicCast<QnNetworkResource>();
    if (camera.isNull())
        return;

    if (pb_camera.has_physicalid())
        camera->setPhysicalId(QString::fromUtf8(pb_camera.physicalid().c_str()));
    camera->setAuth(QString::fromUtf8(pb_camera.login().c_str()), QString::fromUtf8(pb_camera.password().c_str()));


    QnVirtualCameraResourcePtr vCamera = camera.dynamicCast<QnVirtualCameraResource>();
    if (vCamera.isNull())
        return;

    for (int j = 0; j < pb_cameraResource.property_size(); j++)
    {
        const pb::Resource_Property& pb_property = pb_cameraResource.property(j);

        vCamera->setParam(QString::fromUtf8(pb_property.name().c_str()), QString::fromUtf8(pb_property.value().c_str()), QnDomainDatabase);
    }

    vCamera->setScheduleDisabled(pb_camera.scheduledisabled());
    if (pb_camera.has_audioenabled())
        vCamera->setAudioEnabled(pb_camera.audioenabled());

    if (pb_camera.has_manuallyadded())
        vCamera->setManuallyAdded(pb_camera.manuallyadded());

    vCamera->setModel(QString::fromUtf8(pb_camera.model().c_str()));
    vCamera->setFirmware(QString::fromUtf8(pb_camera.firmware().c_str()));
    vCamera->setVendor(QString::fromUtf8(pb_camera.vendor().c_str()));

    vCamera->setMotionType(static_cast<Qn::MotionType>(pb_camera.motiontype()));

    vCamera->setGroupId(QString::fromUtf8(pb_camera.groupid().c_str()));
    vCamera->setGroupName(QString::fromUtf8(pb_camera.groupname().c_str()));
    vCamera->setSecondaryStreamQuality(static_cast<Qn::SecondStreamQuality>(pb_camera.secondaryquality()));
    vCamera->setCameraControlDisabled(pb_camera.controldisabled());
    vCamera->setStatusFlags((QnSecurityCamResource::StatusFlags) pb_camera.statusflags());
    vCamera->setDewarpingParams(QnMediaDewarpingParams::deserialized(pb_camera.dewarpingparams().c_str()));

    if (pb_camera.has_region())
    {
        QList<QnMotionRegion> regions;
        parseMotionRegionList(regions, QString::fromUtf8(pb_camera.region().c_str()));
        while (regions.size() < CL_MAX_CHANNELS)
            regions << QnMotionRegion();

        vCamera->setMotionRegionList(regions, QnDomainMemory);
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
                                        (Qn::RecordingType) pb_scheduleTask.recordtype(),
                                        pb_scheduleTask.beforethreshold(),
                                        pb_scheduleTask.afterthreshold(),
                                        (Qn::StreamQuality) pb_scheduleTask.streamquality(),
                                        pb_scheduleTask.fps(),
                                        pb_scheduleTask.dorecordaudio()
                                       );

            scheduleTasks.append(scheduleTask);
        }

        vCamera->setScheduleTasks(scheduleTasks);
    }
}

void parseCameras(QnNetworkResourceList& cameras, const PbResourceList& pb_cameras, QnResourceFactory& resourceFactory)
{
    for (PbResourceList::const_iterator ci = pb_cameras.begin(); ci != pb_cameras.end(); ++ci)
    {
        const pb::Resource& pb_cameraResource = *ci;

        QnNetworkResourcePtr camera;
        parseCamera(camera, pb_cameraResource, resourceFactory);
        if (camera)
            cameras.append(camera);
        else
            cl_log.log("Can't create resource with id=", ci->id(), cl_logWARNING);
    }
}

void parseServer(QnMediaServerResourcePtr &server, const pb::Resource &pb_serverResource, QnResourceFactory &resourceFactory)
{
    const pb::Server& pb_server = pb_serverResource.GetExtension(pb::Server::resource);

    server = QnMediaServerResourcePtr(new QnMediaServerResource());
    server->setId(pb_serverResource.id());
    server->setName(QString::fromUtf8(pb_serverResource.name().c_str()));
    server->setUrl(QString::fromUtf8(pb_serverResource.url().c_str()));
    server->setGuid(QString::fromUtf8(pb_serverResource.guid().c_str()));
    server->setApiUrl(QString::fromUtf8(pb_server.apiurl().c_str()));
    if (pb_server.has_streamingurl())
        server->setStreamingUrl(QString::fromUtf8(pb_server.streamingurl().c_str()));

    if (pb_server.has_version())
        server->setVersion(QnSoftwareVersion(QString::fromUtf8(pb_server.version().c_str())));

    if (pb_serverResource.has_status())
        server->setStatus(static_cast<QnResource::Status>(pb_serverResource.status()));

    if (pb_server.has_netaddrlist())
    {
        QList<QHostAddress> netAddrList;
        deserializeNetAddrList(netAddrList, QString::fromUtf8(pb_server.netaddrlist().c_str()));
        server->setNetAddrList(netAddrList);
    }

    if (pb_server.has_reserve())
    {
        server->setReserve(pb_server.reserve());
    }

    if (pb_server.has_panicmode())
    {
        server->setPanicMode(QnMediaServerResource::PanicMode(pb_server.panicmode()));
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
            if(pb_storage.has_usedforwriting())
                parameters["usedForWriting"] = QString::number(pb_storage.usedforwriting());

            // This code assumes we already have "Storage" resource type in qnResTypePool
            QnResourcePtr st = resourceFactory.createResource(qnResTypePool->getResourceTypeByName(QLatin1String("Storage"))->getId(), parameters);
            storage = qSharedPointerDynamicCast<QnAbstractStorageResource> (st);
            if (storage)
                storages.append(storage);
        }

        server->setStorages(storages);
    }
}

void parseServers(QnMediaServerResourceList &servers, const PbResourceList &pb_servers, QnResourceFactory &resourceFactory)
{
    for (PbResourceList::const_iterator ci = pb_servers.begin(); ci != pb_servers.end(); ++ci)
    {
        const pb::Resource& pb_serverResource = *ci;

        QnMediaServerResourcePtr server;
        parseServer(server, pb_serverResource, resourceFactory);
        if (server)
            servers.append(server);
        else
            cl_log.log("Can't create resource with id=", ci->id(), cl_logWARNING);
    }
}

void parseLayout(QnLayoutResourcePtr& layout, const pb::Resource& pb_layoutResource, QList<QnLayoutItemDataList>* orderedItems)
{
    const pb::Layout& pb_layout = pb_layoutResource.GetExtension(pb::Layout::resource);

    layout = QnLayoutResourcePtr(new QnLayoutResource());

    if (pb_layoutResource.has_id())
        layout->setId(pb_layoutResource.id());

    if (pb_layoutResource.has_guid())
        layout->setGuid(QString::fromUtf8(pb_layoutResource.guid().c_str()));

    layout->setParentId(pb_layoutResource.parentid());
    layout->setName(QString::fromUtf8(pb_layoutResource.name().c_str()));
    layout->setCellAspectRatio(pb_layout.cellaspectratio());
    layout->setCellSpacing(QSizeF(pb_layout.cellspacingwidth(), pb_layout.cellspacingheight()));
    layout->setUserCanEdit(pb_layout.usercanedit());
    layout->setBackgroundImageFilename(QString::fromUtf8(pb_layout.backgroundimagefilename().c_str()));
    layout->setBackgroundSize(QSize(pb_layout.backgroundwidth(), pb_layout.backgroundheight()));
    layout->setBackgroundOpacity(pb_layout.backgroundopacity());
    layout->setLocked(pb_layout.locked());

    if (pb_layout.item_size() > 0)
    {
        QnLayoutItemDataList items;

        for (int j = 0; j < pb_layout.item_size(); j++)
        {
            const pb::Layout_Item& pb_item = pb_layout.item(j);

            QnLayoutItemData itemData;
            itemData.resource.id = pb_item.resource().id();
            itemData.resource.path = QString::fromUtf8(pb_item.resource().path().c_str());

            itemData.uuid = QUuid(QString::fromUtf8(pb_item.uuid().c_str()));
            itemData.flags = pb_item.flags();
            itemData.combinedGeometry.setLeft(pb_item.left());
            itemData.combinedGeometry.setTop(pb_item.top());
            itemData.combinedGeometry.setRight(pb_item.right());
            itemData.combinedGeometry.setBottom(pb_item.bottom());
            itemData.rotation = pb_item.rotation();
            itemData.contrastParams = ImageCorrectionParams::deserialize(QByteArray(pb_item.contrastparams().c_str()));
            itemData.dewarpingParams = QJson::deserialized<QnItemDewarpingParams>(pb_item.dewarpingparams().c_str());

            if(pb_item.has_zoomtargetuuid())
                itemData.zoomTargetUuid = QUuid(QString::fromUtf8(pb_item.zoomtargetuuid().c_str()));

            if(pb_item.has_zoomleft() && pb_item.has_zoomtop() && pb_item.has_zoomright() && pb_item.has_zoombottom()) {
                itemData.zoomRect.setLeft(pb_item.zoomleft());
                itemData.zoomRect.setTop(pb_item.zoomtop());
                itemData.zoomRect.setRight(pb_item.zoomright());
                itemData.zoomRect.setBottom(pb_item.zoombottom());
            }

            items.append(itemData);
        }

        layout->setItems(items);
        if (orderedItems)
            orderedItems->push_back(items);
    }
}

void parseLayouts(QnLayoutResourceList& layouts, const PbResourceList& pb_layouts, QList<QnLayoutItemDataList>* orderedItems)
{
    for (PbResourceList::const_iterator ci = pb_layouts.begin(); ci != pb_layouts.end(); ++ci)
    {
        const pb::Resource& pb_layoutResource = *ci;
        QnLayoutResourcePtr layout;
        parseLayout(layout, pb_layoutResource, orderedItems);
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
    if (pb_user.has_email())
        user->setEmail(QString::fromUtf8(pb_user.email().c_str()));
    user->setHash(QString::fromUtf8(pb_user.hash().c_str()));
    user->setDigest(QString::fromUtf8(pb_user.digest().c_str()));
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

void parseSettings(QnKvPairList& kvPairs, const PbSettingList& pb_settings)
{
    for (PbSettingList::const_iterator ci = pb_settings.begin(); ci != pb_settings.end(); ++ci)
    {
        QnKvPair kvPair;
        kvPair.setName(QString::fromUtf8(ci->name().c_str()));
        kvPair.setValue(QString::fromUtf8(ci->value().c_str()));

        kvPairs.append(kvPair);
    }
}

} // namespace {}

void serializeCamera_i(pb::Resource& pb_cameraResource, const QnVirtualCameraResourcePtr& cameraPtr)
{
    pb_cameraResource.set_type(pb::Resource_Type_Camera);
    pb::Camera &pb_camera = *pb_cameraResource.MutableExtension(pb::Camera::resource);

    pb_cameraResource.set_id(cameraPtr->getId().toInt());
    pb_cameraResource.set_parentid(cameraPtr->getParentId().toInt());
    pb_cameraResource.set_name(cameraPtr->getName().toUtf8().constData());
    pb_cameraResource.set_resourcetypeid(cameraPtr->getTypeId().toInt());
    pb_cameraResource.set_url(cameraPtr->getUrl().toUtf8().constData());
    pb_camera.set_mac(cameraPtr->getMAC().toString().toUtf8().constData());
    pb_camera.set_physicalid(cameraPtr->getPhysicalId().toUtf8().constData());
    pb_camera.set_model(cameraPtr->getModel().toUtf8().constData());
    pb_camera.set_firmware(cameraPtr->getFirmware().toUtf8().constData());
    pb_camera.set_login(cameraPtr->getAuth().user().toUtf8().constData());
    pb_camera.set_password(cameraPtr->getAuth().password().toUtf8().constData());
    pb_cameraResource.set_disabled(cameraPtr->isDisabled());
    pb_cameraResource.set_status(static_cast<pb::Resource_Status>(cameraPtr->getStatus()));
    pb_camera.set_region(serializeMotionRegionList(cameraPtr->getMotionRegionList()).toUtf8().constData());
    pb_camera.set_scheduledisabled(cameraPtr->isScheduleDisabled());
    pb_camera.set_audioenabled(cameraPtr->isAudioEnabled());
    pb_camera.set_manuallyadded(cameraPtr->isManuallyAdded());
    pb_camera.set_motiontype(static_cast<pb::Camera_MotionType>(cameraPtr->getMotionType()));
    pb_camera.set_groupid(cameraPtr->getGroupId().toUtf8().constData());
    pb_camera.set_groupname(cameraPtr->getGroupName().toUtf8().constData());
    pb_camera.set_vendor(cameraPtr->getVendor().toUtf8().constData());

    pb_camera.set_secondaryquality(static_cast<pb::Camera_SecondaryQuality>(cameraPtr->secondaryStreamQuality()));
    pb_camera.set_controldisabled(cameraPtr->isCameraControlDisabled());
    pb_camera.set_statusflags((int) cameraPtr->statusFlags());
    pb_camera.set_dewarpingparams(QJson::serialized<QnMediaDewarpingParams>(cameraPtr->getDewarpingParams()));

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
    pb_cameraServerItem.set_physicalid(cameraServerItem.physicalId.toUtf8());
    pb_cameraServerItem.set_timestamp(cameraServerItem.timestamp);
    pb_cameraServerItem.set_serverguid(cameraServerItem.mediaServerGuid.toUtf8());
}


int serializeBusinessActionType(BusinessActionType::Value value) {
    switch(value) {
    case BusinessActionType::NotDefined:            return pb::NotDefinedAction;
    case BusinessActionType::CameraOutput:          return pb::CameraOutput;
    case BusinessActionType::CameraOutputInstant:   return pb::CameraOutputInstant;
    case BusinessActionType::Bookmark:              return pb::Bookmark;
    case BusinessActionType::CameraRecording:       return pb::CameraRecording;
    case BusinessActionType::PanicRecording:        return pb::PanicRecording;
    case BusinessActionType::SendMail:              return pb::SendMail;
    case BusinessActionType::Diagnostics:           return pb::Diagnostics;
    case BusinessActionType::ShowPopup:             return pb::ShowPopup;
    case BusinessActionType::PlaySound:             return pb::PlaySound;
    case BusinessActionType::PlaySoundRepeated:     return pb::PlaySoundRepeated;
    case BusinessActionType::SayText:               return pb::SayText;
    }
    return pb::NotDefinedAction;
}

int serializeBusinessEventType(BusinessEventType::Value value) {
    int userEvent = value - BusinessEventType::UserDefined;
    if (userEvent >= 0)
        return (pb::UserDefinedEvent + userEvent);

    int healthMessage = value - BusinessEventType::SystemHealthMessage;
    if (healthMessage >= 0)
        return (pb::SystemHealthMessage + healthMessage);

    switch(value) {
    case BusinessEventType::NotDefined:          return pb::NotDefinedEvent;
    case BusinessEventType::Camera_Motion:       return pb::Camera_Motion;
    case BusinessEventType::Camera_Input:        return pb::Camera_Input;
    case BusinessEventType::Camera_Disconnect:   return pb::Camera_Disconnect;
    case BusinessEventType::Storage_Failure:     return pb::Storage_Failure;
    case BusinessEventType::Network_Issue:       return pb::Network_Issue;
    case BusinessEventType::Camera_Ip_Conflict:  return pb::Camera_Ip_Conflict;
    case BusinessEventType::MediaServer_Failure: return pb::MediaServer_Failure;
    case BusinessEventType::MediaServer_Conflict:return pb::MediaServer_Conflict;
    case BusinessEventType::MediaServer_Started: return pb::MediaServer_Started;
    default:
        break;
    }
    return pb::NotDefinedEvent;
}

void serializeBusinessRule_i(pb::BusinessRule& pb_businessRule, const QnBusinessEventRulePtr& businessRulePtr)
{
    pb_businessRule.set_id(businessRulePtr->id());

    pb_businessRule.set_eventtype((pb::BusinessEventType) serializeBusinessEventType(businessRulePtr->eventType()));
    foreach(QnResourcePtr res, businessRulePtr->eventResources())
        pb_businessRule.add_eventresource(res->getId().toInt());
    pb_businessRule.set_eventcondition(serializeBusinessParams(businessRulePtr->eventParams().toBusinessParams()));
    pb_businessRule.set_eventstate((pb::ToggleStateType)businessRulePtr->eventState());

    pb_businessRule.set_actiontype((pb::BusinessActionType) serializeBusinessActionType(businessRulePtr->actionType()));
    foreach(QnResourcePtr res, businessRulePtr->actionResources())
        pb_businessRule.add_actionresource(res->getId().toInt());
    pb_businessRule.set_actionparams(serializeBusinessParams(businessRulePtr->actionParams().toBusinessParams()));

    pb_businessRule.set_aggregationperiod(businessRulePtr->aggregationPeriod());
    pb_businessRule.set_disabled(businessRulePtr->disabled());
    pb_businessRule.set_comments(businessRulePtr->comments().toUtf8());
    pb_businessRule.set_schedule(businessRulePtr->schedule().toUtf8());
    pb_businessRule.set_system(businessRulePtr->system());
}

void serializeKvPair_i(int resourceId, pb::KvPair& pb_kvPair, const QnKvPair& kvPair)
{
    pb_kvPair.set_resourceid(resourceId);
    pb_kvPair.set_name(kvPair.name().toUtf8());
    pb_kvPair.set_value(kvPair.value().toUtf8());
}

void serializeSetting_i(pb::Setting& pb_setting, const QnKvPair& kvPair)
{
    pb_setting.set_name(kvPair.name().toUtf8());
    pb_setting.set_value(kvPair.value().toUtf8());
}

void serializeLayout_i(pb::Resource& pb_layoutResource, const QnLayoutResourcePtr& layoutIn)
{
    pb_layoutResource.set_type(pb::Resource_Type_Layout);
    pb::Layout &pb_layout = *pb_layoutResource.MutableExtension(pb::Layout::resource);

    pb_layoutResource.set_parentid(layoutIn->getParentId().toInt());
    pb_layoutResource.set_name(layoutIn->getName().toUtf8().constData());
    pb_layoutResource.set_guid(layoutIn->getGuid().toLatin1().constData());

    pb_layout.set_cellaspectratio(layoutIn->cellAspectRatio());
    pb_layout.set_cellspacingwidth(layoutIn->cellSpacing().width());
    pb_layout.set_cellspacingheight(layoutIn->cellSpacing().height());
    pb_layout.set_usercanedit(layoutIn->userCanEdit());
    pb_layout.set_backgroundimagefilename(layoutIn->backgroundImageFilename().toUtf8().constData());
    pb_layout.set_backgroundwidth(layoutIn->backgroundSize().width());
    pb_layout.set_backgroundheight(layoutIn->backgroundSize().height());
    pb_layout.set_backgroundopacity(layoutIn->backgroundOpacity());
    pb_layout.set_locked(layoutIn->locked());

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
            pb_item.set_contrastparams(itemIn.contrastParams.serialize().constData());
            pb_item.set_dewarpingparams(QJson::serialized<QnItemDewarpingParams>(itemIn.dewarpingParams));

            pb_item.set_zoomtargetuuid(itemIn.zoomTargetUuid.toString().toUtf8().constData());
            pb_item.set_zoomleft(itemIn.zoomRect.left());
            pb_item.set_zoomtop(itemIn.zoomRect.top());
            pb_item.set_zoomright(itemIn.zoomRect.right());
            pb_item.set_zoombottom(itemIn.zoomRect.bottom());
        }
    }
}

void serializeLicense_i(pb::License& pb_license, const QnLicensePtr& license)
{
    // We can't remove it by now, as it's required in .proto
    // Will remove it ocasionally.
    pb_license.set_name("");
    pb_license.set_hwid(""); 
    pb_license.set_signature("");
    /////////

    pb_license.set_key(license->key().constData());
    pb_license.set_cameracount(license->cameraCount());
    pb_license.set_rawlicense(license->rawLicense().constData());
}

BusinessEventType::Value parsePbBusinessEventType(int pbValue) {
    int userEvent = pbValue - pb::UserDefinedEvent;
    if (userEvent >= 0)
        return BusinessEventType::Value(BusinessEventType::UserDefined + userEvent);

    int healthMessage = pbValue - pb::SystemHealthMessage;
    if (healthMessage >= 0)
        return BusinessEventType::Value((BusinessEventType::SystemHealthMessage + healthMessage));

    switch(pbValue) {
    case pb::NotDefinedEvent:       return BusinessEventType::NotDefined;
    case pb::Camera_Motion:         return BusinessEventType::Camera_Motion;
    case pb::Camera_Input:          return BusinessEventType::Camera_Input;
    case pb::Camera_Disconnect:     return BusinessEventType::Camera_Disconnect;
    case pb::Storage_Failure:       return BusinessEventType::Storage_Failure;
    case pb::Network_Issue:         return BusinessEventType::Network_Issue;
    case pb::Camera_Ip_Conflict:    return BusinessEventType::Camera_Ip_Conflict;
    case pb::MediaServer_Failure:   return BusinessEventType::MediaServer_Failure;
    case pb::MediaServer_Conflict:  return BusinessEventType::MediaServer_Conflict;
    case pb::MediaServer_Started:  return BusinessEventType::MediaServer_Started;
    }
    return BusinessEventType::NotDefined;
}

BusinessActionType::Value parsePbBusinessActionType(int pbValue) {
    switch (pbValue) {
    case pb::NotDefinedAction:      return BusinessActionType::NotDefined;
    case pb::CameraOutput:          return BusinessActionType::CameraOutput;
    case pb::CameraOutputInstant:   return BusinessActionType::CameraOutputInstant;
    case pb::Bookmark:              return BusinessActionType::Bookmark;
    case pb::CameraRecording:       return BusinessActionType::CameraRecording;
    case pb::PanicRecording:        return BusinessActionType::PanicRecording;
    case pb::SendMail:              return BusinessActionType::SendMail;
    case pb::Diagnostics:           return BusinessActionType::Diagnostics;
    case pb::ShowPopup:             return BusinessActionType::ShowPopup;
    case pb::PlaySound:             return BusinessActionType::PlaySound;
    case pb::PlaySoundRepeated:     return BusinessActionType::PlaySoundRepeated;
    case pb::SayText:               return BusinessActionType::SayText;
    }
    return BusinessActionType::NotDefined;
}



void QnApiPbSerializer::deserializeCameras(QnNetworkResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    pb::Resources pb_cameras;
    if (!pb_cameras.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized cameras."));

    parseCameras(cameras, pb_cameras.resource(), resourceFactory);
}

void QnApiPbSerializer::deserializeServers(QnMediaServerResourceList& servers, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    pb::Resources pb_servers;
    if (!pb_servers.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized servers."));

    parseServers(servers, pb_servers.resource(), resourceFactory);
}

void QnApiPbSerializer::deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data, QList<QnLayoutItemDataList>* orderedItems)
{
    pb::Resources pb_layouts;
    if (!pb_layouts.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized layouts."));

    parseLayouts(layouts, pb_layouts.resource(), orderedItems);
}

void QnApiPbSerializer::deserializeLayout(QnLayoutResourcePtr& layout, const QByteArray& data, QList<QnLayoutItemDataList>* orderedItems)
{
    QnLayoutResourceList layouts;
    deserializeLayouts(layouts, data, orderedItems);

    if (layouts.isEmpty())
        layout = QnLayoutResourcePtr();
    else
        layout = layouts.at(0);
}

void QnApiPbSerializer::deserializeUsers(QnUserResourceList& users, const QByteArray& data)
{
    pb::Resources pb_users;
    if (!pb_users.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized users."));

    parseUsers(users, pb_users.resource());
}

void QnApiPbSerializer::deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory)
{
    pb::Resources pb_resources;
    if (!pb_resources.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized resources."));

    parseResources(resources, pb_resources.resource(), resourceFactory);
}

void QnApiPbSerializer::deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data)
{
    pb::ResourceTypes pb_resourceTypes;
    if (!pb_resourceTypes.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized resource types."));

    parseResourceTypes(resourceTypes, pb_resourceTypes.resourcetype());
}

void QnApiPbSerializer::deserializeLicenses(QnLicenseList &licenses, const QByteArray &data)
{
    pb::Licenses pb_licenses;
    if (!pb_licenses.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized licenses."));

    parseLicenses(licenses, pb_licenses.license());
}

void QnApiPbSerializer::deserializeCameraHistoryList(QnCameraHistoryList &cameraHistoryList, const QByteArray &data)
{
    pb::CameraServerItems pb_csis;

    if (!pb_csis.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized camera history."));

    parseCameraServerItems(cameraHistoryList, pb_csis.cameraserveritem());
}

void QnApiPbSerializer::deserializeKvPairs(QnKvPairListsById& kvPairs, const QByteArray& data)
{
    pb::KvPairs pb_kvPairs;

    if (!pb_kvPairs.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized key-value pairs."));

    parseKvPairs(kvPairs, pb_kvPairs.kvpair());
}

void QnApiPbSerializer::deserializeSettings(QnKvPairList& kvPairs, const QByteArray& data)
{
    pb::Settings pb_settings;

    if (!pb_settings.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized settings."));

    parseSettings(kvPairs, pb_settings.setting());
}

void QnApiPbSerializer::deserializeConnectInfo(QnConnectionInfoPtr& connectInfo, const QByteArray& data)
{
    pb::ConnectInfo pb_connectInfo;

    if (!pb_connectInfo.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized connection information."));

    connectInfo->version = QnSoftwareVersion(QString::fromUtf8(pb_connectInfo.version().c_str()));

    typedef google::protobuf::RepeatedPtrField<pb::CompatibilityItem> PbCompatibilityItemList;
    PbCompatibilityItemList items = pb_connectInfo.compatibilityitems().item();

    for (PbCompatibilityItemList::const_iterator ci = items.begin(); ci != items.end(); ++ci)
    {
        connectInfo->compatibilityItems.append(QnCompatibilityItem(QString::fromUtf8(ci->ver1().c_str()),
            QString::fromUtf8(ci->comp1().c_str()), QString::fromUtf8(ci->ver2().c_str())));
    }
    connectInfo->proxyPort = pb_connectInfo.proxyport();
    connectInfo->ecsGuid = QString::fromUtf8(pb_connectInfo.ecsguid().c_str());
    connectInfo->publicIp = QString::fromUtf8(pb_connectInfo.publicip().c_str());
    connectInfo->brand = QString::fromUtf8(pb_connectInfo.brand().c_str());
}

void QnApiPbSerializer::deserializeBusinessRules(QnBusinessEventRuleList &businessRules, const QByteArray &data)
{
    pb::BusinessRules pb_businessRules;
    if (!pb_businessRules.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized event/action rules."));

    parseBusinessRules(businessRules, pb_businessRules.businessrule());
}

void QnApiPbSerializer::deserializeBusinessAction(QnAbstractBusinessActionPtr& businessAction, const QByteArray& data)
{
    pb::BusinessAction pb_businessAction;
    if (!pb_businessAction.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized action."));

    parseBusinessAction(businessAction, pb_businessAction);
}

void QnApiPbSerializer::deserializeBusinessActionVector(QnBusinessActionDataListPtr& businessActionList, const QByteArray& data)
{
    pb::BusinessActionList pb_businessActionList;
    if (!pb_businessActionList.ParseFromArray(data.data(), data.size()))
        throw QnSerializationException(tr("Cannot parse serialized actions."));

    parseBusinessActionVector(businessActionList, pb_businessActionList);
}

void QnApiPbSerializer::serializeCameras(const QnVirtualCameraResourceList& cameras, QByteArray& data)
{
    pb::Resources pb_cameras;

    foreach(QnVirtualCameraResourcePtr cameraPtr, cameras)
        serializeCamera_i(*pb_cameras.add_resource(), cameraPtr);

    std::string str;
    pb_cameras.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeLicense(const QnLicensePtr& license, QByteArray& data)
{
    pb::Licenses pb_licenses;

    pb::License& pb_license = *(pb_licenses.add_license());
    serializeLicense_i(pb_license, license);

    std::string str;
    pb_licenses.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeLicenses(const QList<QnLicensePtr>& licenses, QByteArray& data)
{
    pb::Licenses pb_licenses;

    foreach(QnLicensePtr license, licenses)
        serializeLicense_i(*pb_licenses.add_license(), license);

    std::string str;
    pb_licenses.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeServer(const QnMediaServerResourcePtr& serverPtr, QByteArray& data)
{
    pb::Resources pb_servers;

    pb::Resource& pb_serverResource = *pb_servers.add_resource();
    pb_serverResource.set_type(pb::Resource_Type_Server);
    pb::Server &pb_server = *pb_serverResource.MutableExtension(pb::Server::resource);

    pb_serverResource.set_id(serverPtr->getId().toInt());
    pb_serverResource.set_name(serverPtr->getName().toUtf8().constData());
    pb_serverResource.set_url(serverPtr->getUrl().toUtf8().constData());
    pb_serverResource.set_guid(serverPtr->getGuid().toLatin1().constData());
    pb_server.set_apiurl(serverPtr->getApiUrl().toUtf8().constData());
    pb_server.set_streamingurl(serverPtr->getStreamingUrl().toUtf8().constData());
    pb_serverResource.set_status(static_cast<pb::Resource_Status>(serverPtr->getStatus()));
    pb_server.set_version(serverPtr->getVersion().toString().toUtf8().constData());

    if (!serverPtr->getNetAddrList().isEmpty())
        pb_server.set_netaddrlist(serializeNetAddrList(serverPtr->getNetAddrList()).toUtf8().constData());

    pb_server.set_reserve(serverPtr->getReserve());
    pb_server.set_panicmode((int) serverPtr->getPanicMode());

    if (!serverPtr->getStorages().isEmpty()) {
        foreach (const QnAbstractStorageResourcePtr& storagePtr, serverPtr->getStorages()) {
            pb::Server_Storage& pb_storage = *pb_server.add_storage();

            pb_storage.set_id(storagePtr->getId().toInt());
            pb_storage.set_name(storagePtr->getName().toUtf8().constData());
            pb_storage.set_url(storagePtr->getUrl().toUtf8().constData());
            pb_storage.set_spacelimit(storagePtr->getSpaceLimit());
            pb_storage.set_usedforwriting(storagePtr->isUsedForWriting());
        }
    }

    std::string str;
    pb_servers.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
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
    pb_user.set_hash(userPtr->getHash().toUtf8().constData());
    pb_user.set_rights(userPtr->getPermissions());
    pb_user.set_isadmin(userPtr->isAdmin());
    pb_user.set_email(userPtr->getEmail().toUtf8().constData());
    pb_userResource.set_guid(userPtr->getGuid().toUtf8().constData());

    std::string str;
    pb_users.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeCamera(const QnVirtualCameraResourcePtr& cameraPtr, QByteArray& data)
{
    pb::Resources pb_cameras;
    serializeCamera_i(*pb_cameras.add_resource(), cameraPtr);

    std::string str;
    pb_cameras.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeLayout(const QnLayoutResourcePtr& layout, QByteArray& data)
{
    pb::Resources pb_layouts;
    serializeLayout_i(*pb_layouts.add_resource(), layout);

    std::string str;
    pb_layouts.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data)
{
    pb::Resources pb_layouts;

    foreach(QnLayoutResourcePtr layout, layouts)
        serializeLayout_i(*pb_layouts.add_resource(), layout);

    std::string str;
    pb_layouts.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeCameraServerItem(const QnCameraHistoryItem& cameraServerItem, QByteArray &data)
{
    pb::CameraServerItems pb_cameraServerItems;
    serializeCameraServerItem_i(*pb_cameraServerItems.add_cameraserveritem(), cameraServerItem);

    std::string str;
    pb_cameraServerItems.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeBusinessRules(const QnBusinessEventRuleList &businessRules, QByteArray &data)
{
    pb::BusinessRules pb_businessRules;

    foreach(QnBusinessEventRulePtr businessRulePtr, businessRules)
        serializeBusinessRule_i(*pb_businessRules.add_businessrule(), businessRulePtr);

    std::string str;
    pb_businessRules.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeBusinessRule(const QnBusinessEventRulePtr &businessRule, QByteArray &data)
{
    pb::BusinessRules pb_businessRules;

    serializeBusinessRule_i(*pb_businessRules.add_businessrule(), businessRule);

    std::string str;
    pb_businessRules.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeEmail(const QStringList& to, const QString& subject, const QString& message, const QnEmailAttachmentList& attachments, int timeout, QByteArray& data)
{
    pb::Emails pb_emails;
    pb_emails.set_timeout(timeout);

    pb::Email& email = *(pb_emails.add_email());

    foreach (const QString& addr, to)
        email.add_to(addr.toUtf8().constData());

    email.set_subject(subject.toUtf8().constData());
    email.set_body(message.toUtf8().constData());

    foreach (QnEmailAttachmentPtr attachment, attachments) {
        pb::Attachment& pb_attachment = *email.add_attachment();
        pb_attachment.set_filename(attachment->filename.toUtf8().constData());
        pb_attachment.set_content(attachment->content.data(), attachment->content.length());
        pb_attachment.set_mimetype(attachment->mimetype.toUtf8().constData());
    }

    std::string str;
    pb_emails.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeBusinessAction(const QnAbstractBusinessActionPtr &action, QByteArray &data)
{
    pb::BusinessAction pb_businessAction;

    pb_businessAction.set_actiontype((pb::BusinessActionType) serializeBusinessActionType(action->actionType()));
    foreach(QnResourcePtr res, action->getResources()) {
        if (res)
            pb_businessAction.add_actionresource(res->getId());
    }
    pb_businessAction.set_actionparams(action->getParams().serialize());
    pb_businessAction.set_runtimeparams(action->getRuntimeParams().serialize());
    pb_businessAction.set_businessruleid(action->getBusinessRuleId().toInt());
    pb_businessAction.set_togglestate((pb::ToggleStateType) action->getToggleState());
    pb_businessAction.set_aggregationcount(action->getAggregationCount());

    std::string str;
    pb_businessAction.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeBusinessActionList(const QnAbstractBusinessActionList &actions, QByteArray &data)
{
    pb::BusinessActionList pb_businessActionList;

    for (int i = 0; i < actions.size(); ++i)
    {
        pb::BusinessAction* ba = pb_businessActionList.add_businessaction();
        ba->set_actiontype((pb::BusinessActionType) serializeBusinessActionType(actions[i]->actionType()));
        foreach(QnResourcePtr res, actions[i]->getResources()) {
            if (res)
                ba->add_actionresource(res->getId());
        }
        ba->set_actionparams(actions[i]->getParams().serialize());
        ba->set_runtimeparams(actions[i]->getRuntimeParams().serialize());
        ba->set_businessruleid(actions[i]->getBusinessRuleId().toInt());
        ba->set_togglestate((pb::ToggleStateType) actions[i]->getToggleState());
        ba->set_aggregationcount(actions[i]->getAggregationCount());

    }

    std::string str;
    pb_businessActionList.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeKvPair(const QnResourcePtr& resource, const QnKvPair& kvPair, QByteArray& data)
{
    pb::KvPairs pb_kvPairs;
    serializeKvPair_i(resource->getId(), *pb_kvPairs.add_kvpair(), kvPair);

    std::string str;
    pb_kvPairs.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeKvPairs(int resourceId, const QnKvPairList& kvPairs, QByteArray& data)
{
    pb::KvPairs pb_kvPairs;

    foreach(const QnKvPair &kvPair, kvPairs)
        serializeKvPair_i(resourceId, *pb_kvPairs.add_kvpair(), kvPair);

    std::string str;
    pb_kvPairs.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void QnApiPbSerializer::serializeSettings(const QnKvPairList& kvPairs, QByteArray& data)
{
    pb::Settings pb_settings;

    foreach(const QnKvPair &kvPair, kvPairs)
        serializeSetting_i(*pb_settings.add_setting(), kvPair);

    std::string str;
    pb_settings.SerializeToString(&str);
    data = QByteArray(str.data(), (int) str.length());
}

void parseResource(QnResourcePtr& resource, const pb::Resource& pb_resource, QnResourceFactory& resourceFactory)
{
    switch (pb_resource.type())
    {
        case pb::Resource_Type_Camera:
        {
            QnNetworkResourcePtr camera;
            parseCamera(camera, pb_resource, resourceFactory);
            resource = camera;
            break;
        }
        case pb::Resource_Type_Server:
        {
            QnMediaServerResourcePtr server;
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
            parseLayout(layout, pb_resource, 0);
            resource = layout;
            break;
        }
    }
}

// TODO: #Ivan. Temporary code
QByteArray combineV1LicenseBlock(const QString& name, const QString& serial, const QString& hwid, int count, const QString& signature)
{
    return (QString() +
        QString(QLatin1String("NAME=")) + name + QLatin1String("\n") + 
        QLatin1String("SERIAL=") +  serial + QLatin1String("\n") + 
        QLatin1String("HWID=") + hwid + QLatin1String("\n") + 
        QLatin1String("COUNT=") + QString::number(count) + QLatin1String("\n") +
        QLatin1String("SIGNATURE=") + signature).toUtf8();
}

void parseLicense(QnLicensePtr& license, const pb::License& pb_license)
{
    // In earlier version rawlicense could be empty, but we're not compatible with them, so it's always filled.
    license = QnLicensePtr(new QnLicense(pb_license.rawlicense().c_str()));
}

void parseCameraServerItem(QnCameraHistoryItemPtr& historyItem, const pb::CameraServerItem& pb_cameraServerItem)
{
    historyItem = QnCameraHistoryItemPtr(new QnCameraHistoryItem(
                                            QString::fromUtf8(pb_cameraServerItem.physicalid().c_str()),
                                            pb_cameraServerItem.timestamp(),
                                            QString::fromUtf8(pb_cameraServerItem.serverguid().c_str())
                                        ));
}

void parseBusinessRule(QnBusinessEventRulePtr& businessRule, const pb::BusinessRule& pb_businessRule)
{
    businessRule = QnBusinessEventRulePtr(new QnBusinessEventRule());

    businessRule->setId(pb_businessRule.id());
    businessRule->setEventType(parsePbBusinessEventType(pb_businessRule.eventtype()));

    QnResourceList eventResources;
    for (int i = 0; i < pb_businessRule.eventresource_size(); i++) {
        QnResourcePtr resource = qnResPool->getResourceById(pb_businessRule.eventresource(i), QnResourcePool::AllResources);
        if (resource)
            eventResources << resource;
        else
            qWarning() << "NULL event resource while reading rule" << pb_businessRule.id();
    }
    businessRule->setEventResources(eventResources);
    QnBusinessParams bParams = deserializeBusinessParams(pb_businessRule.eventcondition().c_str());
    businessRule->setEventParams(QnBusinessEventParameters::fromBusinessParams(bParams));
    businessRule->setEventState((Qn::ToggleState)pb_businessRule.eventstate());

    businessRule->setActionType(parsePbBusinessActionType(pb_businessRule.actiontype()));
    QnResourceList actionResources;
    for (int i = 0; i < pb_businessRule.actionresource_size(); i++) {//destination resource can belong to another server
        QnResourcePtr resource = qnResPool->getResourceById(pb_businessRule.actionresource(i), QnResourcePool::AllResources);
        if (resource)
            actionResources << resource;
        else
            qWarning() << "NULL action resource while reading rule" << pb_businessRule.id();
    }
    businessRule->setActionResources(actionResources);
    bParams = deserializeBusinessParams(pb_businessRule.actionparams().c_str());
    businessRule->setActionParams(QnBusinessActionParameters::fromBusinessParams(bParams));

    businessRule->setAggregationPeriod(pb_businessRule.aggregationperiod());
    businessRule->setDisabled(pb_businessRule.disabled());
    businessRule->setComments(QString::fromUtf8(pb_businessRule.comments().c_str()));
    businessRule->setSchedule(QString::fromUtf8(pb_businessRule.schedule().c_str()));
    businessRule->setSystem(pb_businessRule.system());
}

void parseBusinessRules(QnBusinessEventRuleList& businessRules, const PbBusinessRuleList& pb_businessRules) {
    for (PbBusinessRuleList::const_iterator ci = pb_businessRules.begin(); ci != pb_businessRules.end(); ++ci)
    {
        const pb::BusinessRule& pb_businessRule = *ci;

        QnBusinessEventRulePtr businessRule;
        parseBusinessRule(businessRule, pb_businessRule);
        businessRules.append(businessRule);
    }
}

void parseBusinessAction(QnAbstractBusinessActionPtr& businessAction, const pb::BusinessAction& pb_businessAction)
{
    QnBusinessEventParameters runtimeParams;
    if (pb_businessAction.has_runtimeparams())
        runtimeParams = QnBusinessEventParameters::deserialize(pb_businessAction.runtimeparams().c_str());
    businessAction = QnBusinessActionFactory::createAction(
                parsePbBusinessActionType(pb_businessAction.actiontype()),
                runtimeParams);

    QnResourceList resources;
    for (int i = 0; i < pb_businessAction.actionresource_size(); i++) //destination resource can belong to another server
        resources << qnResPool->getResourceById(pb_businessAction.actionresource(i), QnResourcePool::AllResources);
    businessAction->setResources(resources);

    businessAction->setParams(QnBusinessActionParameters::deserialize(pb_businessAction.actionparams().c_str()));
    businessAction->setBusinessRuleId(pb_businessAction.businessruleid());
    businessAction->setToggleState((Qn::ToggleState) pb_businessAction.togglestate());
    businessAction->setAggregationCount(pb_businessAction.aggregationcount());
}

void parseBusinessActionList(QnAbstractBusinessActionList& businessActionList, const pb::BusinessActionList& pb_businessActionList)
{
    for (int i = 0; i < pb_businessActionList.businessaction_size(); ++i)
    {
        const pb::BusinessAction& pb_businessAction = pb_businessActionList.businessaction(i);

        QnBusinessEventParameters runtimeParams;
        if (pb_businessAction.has_runtimeparams())
            runtimeParams = QnBusinessEventParameters::deserialize(pb_businessAction.runtimeparams().c_str());
        QnAbstractBusinessActionPtr businessAction = QnBusinessActionFactory::createAction(
            parsePbBusinessActionType(pb_businessAction.actiontype()),
            runtimeParams);

        QnResourceList resources;
        for (int i = 0; i < pb_businessAction.actionresource_size(); i++) //destination resource can belong to another server
            resources << qnResPool->getResourceById(pb_businessAction.actionresource(i), QnResourcePool::AllResources);
        businessAction->setResources(resources);

        businessAction->setParams(QnBusinessActionParameters::deserialize(pb_businessAction.actionparams().c_str()));
        businessAction->setBusinessRuleId(pb_businessAction.businessruleid());
        businessAction->setToggleState((Qn::ToggleState) pb_businessAction.togglestate());
        businessAction->setAggregationCount(pb_businessAction.aggregationcount());

        businessActionList << businessAction;
    }
}

void parseBusinessActionVector(QnBusinessActionDataListPtr& businessActionVectorPtr, const pb::BusinessActionList& pb_businessActionList)
{
    QnBusinessActionDataList& businessActionVector = *(businessActionVectorPtr.data());

    businessActionVector.resize(pb_businessActionList.businessaction_size());
    for (int i = 0; i < pb_businessActionList.businessaction_size(); ++i)
    {
        const pb::BusinessAction& pb_businessAction = pb_businessActionList.businessaction(i);

        QnBusinessActionData& businessAction = businessActionVector[i];
        businessAction.setActionType((BusinessActionType::Value) pb_businessAction.actiontype());
        if (pb_businessAction.has_runtimeparams()) 
            businessAction.setRuntimeParams(QnBusinessEventParameters::deserialize(pb_businessAction.runtimeparams().c_str()));

        QnResourceList resources;
        //for (int i = 0; i < pb_businessAction.actionresource_size(); i++) //destination resource can belong to another server
        //    resources << qnResPool->getResourceById(pb_businessAction.actionresource(i), QnResourcePool::AllResources);
        //businessAction.setResources(resources);

        //businessAction.setParams(QnBusinessActionParameters::deserialize(pb_businessAction.actionparams().c_str()));
        businessAction.setBusinessRuleId(pb_businessAction.businessruleid());
        //businessAction.setToggleState((Qn::ToggleState) pb_businessAction.togglestate());
        businessAction.setAggregationCount(pb_businessAction.aggregationcount());
    }
}

void parseLicenses(QnLicenseList& licenses, const PbLicenseList& pb_licenses)
{
    for (PbLicenseList::const_iterator ci = pb_licenses.begin(); ci != pb_licenses.end(); ++ci)
    {
        const pb::License& pb_license = *ci;

        QnLicensePtr license;
        // Parse license and validate its signature
        if (!pb_license.rawlicense().empty() || !pb_license.signature().empty())
            parseLicense(license, pb_license);

        // Verify that license is valid and for our hardwareid
        if (license)
            licenses.append(license);
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

void parseCameraServerItems(QnCameraHistoryList& cameraServerItems, const PbCameraServerItemList& pb_cameraServerItems)
{
    typedef QMap<qint64, QString> TimestampGuid;
    typedef QMap<QString, TimestampGuid> HistoryType;

    // CameraMAC -> (Timestamp -> ServerGuid)
    HistoryType history;

    // Fill temporary history map
    for (PbCameraServerItemList::const_iterator ci = pb_cameraServerItems.begin(); ci != pb_cameraServerItems.end(); ++ci)
    {
        const pb::CameraServerItem& pb_item = *ci;
        history[QString::fromUtf8(pb_item.physicalid().c_str())][pb_item.timestamp()] = QString::fromUtf8(pb_item.serverguid().c_str());
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

void parseKvPairs(QnKvPairListsById& kvPairs, const PbKvPairList& pb_kvPairs)
{
    for (PbKvPairList::const_iterator ci = pb_kvPairs.begin(); ci != pb_kvPairs.end(); ++ci)
    {
        QnKvPair kvPair;
        kvPair.setName(QString::fromUtf8(ci->name().c_str()));
        kvPair.setValue(QString::fromUtf8(ci->value().c_str()));

        kvPairs[ci->resourceid()].append(kvPair);
    }
}
