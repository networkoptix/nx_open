#include "message.h"
#include "message.pb.h"

#include <api/serializer/pb_serializer.h>
#include <api/app_server_connection.h>

#include <core/resource/resource.h>

namespace {
    typedef google::protobuf::RepeatedPtrField<pb::Resource>            PbResourceList;
    typedef google::protobuf::RepeatedPtrField<pb::ResourceType>        PbResourceTypeList;
    typedef google::protobuf::RepeatedPtrField<pb::License>             PbLicenseList;
    typedef google::protobuf::RepeatedPtrField<pb::CameraServerItem>    PbCameraServerItemList;
    typedef google::protobuf::RepeatedPtrField<pb::BusinessRule>        PbBusinessRuleList;
    typedef google::protobuf::RepeatedPtrField<pb::KvPair>              PbKvPairList;
}

void parseResource(QnResourcePtr& resource, const pb::Resource& pb_resource, QnResourceFactory& resourceFactory);
void parseLicense(QnLicensePtr& license, const pb::License& pb_license, const QByteArray& oldHardwareId);
void parseCameraServerItem(QnCameraHistoryItemPtr& historyItem, const pb::CameraServerItem& pb_cameraServerItem);
void parseBusinessRule(QnBusinessEventRulePtr& businessRule, const pb::BusinessRule& pb_businessRule);
void parseBusinessRules(QnBusinessEventRuleList& businessRules, const PbBusinessRuleList& pb_businessRules);
void parseBusinessAction(QnAbstractBusinessActionPtr& businessAction, const pb::BusinessAction& pb_businessAction);

void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const PbResourceTypeList& pb_resourceTypes);
void parseResources(QnResourceList& resources, const PbResourceList& pb_resources, QnResourceFactory& resourceFactory);
void parseLicenses(QnLicenseList& licenses, const PbLicenseList& pb_licenses);
void parseCameraServerItems(QnCameraHistoryList& cameraServerItems, const PbCameraServerItemList& pb_cameraServerItems);
void parseKvPairs(QnKvPairListsById& kvPairs, const PbKvPairList& pb_kvPairs);
void parseVideoWallControl(QnVideoWallControlMessage &controlMessage, const pb::VideoWallControl& pb_videoWallControl);

namespace Qn {
    QString toString(const Message_Type val) {
        switch(val) {
        case Message_Type_Initial:                      return QLatin1String("Initial");
        case Message_Type_Ping:                         return QLatin1String("Ping");
        case Message_Type_ResourceChange:               return QLatin1String("ResourceChange");
        case Message_Type_ResourceDelete:               return QLatin1String("ResourceDelete");
        case Message_Type_ResourceStatusChange:         return QLatin1String("ResourceStatusChange");
        case Message_Type_ResourceDisabledChange:       return QLatin1String("ResourceDisabledChange");
        case Message_Type_License:                      return QLatin1String("License");
        case Message_Type_CameraServerItem:             return QLatin1String("CameraServerItem");
        case Message_Type_BusinessRuleInsertOrUpdate:   return QLatin1String("BusinessRuleInsertOrUpdate");
        case Message_Type_BusinessRuleDelete:           return QLatin1String("BusinessRuleDelete");
        case Message_Type_BroadcastBusinessAction:      return QLatin1String("BroadcastBusinessAction");
        case Message_Type_FileAdd:                      return QLatin1String("FileAdd");
        case Message_Type_FileRemove:                   return QLatin1String("FileRemove");
        case Message_Type_FileUpdate:                   return QLatin1String("FileUpdate");
        case Message_Type_RuntimeInfoChange:            return QLatin1String("RuntimeInfoChange");
        case Message_Type_BusinessRuleReset:            return QLatin1String("BusinessRuleReset");
        case Message_Type_KvPairChange:                 return QLatin1String("KvPairChang");
        case Message_Type_KvPairDelete:                 return QLatin1String("KvPairDelete");
        case Message_Type_VideoWallControl:             return QLatin1String("VideoWallControl");
        default:
            return lit("Unknown %1").arg((int)val);
        }
    }
} //Qn namespace

bool QnMessage::load(const pb::Message &message)
{
    messageType = (Qn::Message_Type)message.type();
    pb::Message_Type msgType = message.type();
    seqNumber = message.seqnumber();

    switch (msgType)
    {
        case pb::Message_Type_ResourceChange:
        {
            const pb::ResourceMessage& resourceMessage = message.GetExtension(pb::ResourceMessage::message);
            parseResource(resource, resourceMessage.resource(), *QnAppServerConnectionFactory::defaultFactory());
            break;
        }
        case pb::Message_Type_ResourceDisabledChange:
        {
            const pb::ResourceMessage& resourceMessage = message.GetExtension(pb::ResourceMessage::message);
            resourceId = resourceMessage.resource().id();
            resourceGuid = QString::fromStdString(resourceMessage.resource().guid());
            resourceDisabled = resourceMessage.resource().disabled();
            break;
        }
        case pb::Message_Type_ResourceStatusChange:
        {
            const pb::ResourceMessage& resourceMessage = message.GetExtension(pb::ResourceMessage::message);
            resourceId = resourceMessage.resource().id();
            resourceGuid = QString::fromStdString(resourceMessage.resource().guid());
            resourceStatus = static_cast<QnResource::Status>(resourceMessage.resource().status());
            break;
        }
        case pb::Message_Type_ResourceDelete:
        {
            const pb::ResourceMessage& resourceMessage = message.GetExtension(pb::ResourceMessage::message);
            resourceId = resourceMessage.resource().id();
            resourceGuid = QString::fromStdString(resourceMessage.resource().guid());
            break;
        }
        case pb::Message_Type_License:
        {
            const pb::LicenseMessage& licenseMessage = message.GetExtension(pb::LicenseMessage::message);
            parseLicense(license, licenseMessage.license(), qnLicensePool->oldHardwareId());
            break;
        }
        case pb::Message_Type_CameraServerItem:
        {
            const pb::CameraServerItemMessage& cameraServerItemMessage = message.GetExtension(pb::CameraServerItemMessage::message);
            parseCameraServerItem(cameraServerItem, cameraServerItemMessage.cameraserveritem());
            break;
        }
        case pb::Message_Type_Initial:
        {
            const pb::InitialMessage& initialMessage = message.GetExtension(pb::InitialMessage::message);
            systemName = QString::fromUtf8(initialMessage.systemname().c_str());
            sessionKey = initialMessage.sessionkey().c_str();
            oldHardwareId = initialMessage.oldhardwareid().c_str();
            hardwareId1 = initialMessage.hardwareid1().c_str();
            hardwareId2 = initialMessage.hardwareid2().c_str();
            hardwareId3 = initialMessage.hardwareid3().c_str();
            publicIp = QString::fromStdString(initialMessage.publicip());

            parseResourceTypes(resourceTypes, initialMessage.resourcetype());
            qnResTypePool->replaceResourceTypeList(resourceTypes);

            parseResources(resources, initialMessage.resource(), *QnAppServerConnectionFactory::defaultFactory());
            parseLicenses(licenses, initialMessage.license());
            parseCameraServerItems(cameraServerItems, initialMessage.cameraserveritem());
            parseKvPairs(kvPairs, initialMessage.kvpair());
            break;
        }
        case pb::Message_Type_Ping:
            break;
        case pb::Message_Type_BusinessRuleChange:
        {
            const pb::BusinessRuleMessage& businessRuleMessage = message.GetExtension(pb::BusinessRuleMessage::message);
            parseBusinessRule(businessRule, businessRuleMessage.businessrule());
            break;
        }
        case pb::Message_Type_BusinessRuleDelete:
        {
            const pb::BusinessRuleMessage& businessRuleMessage = message.GetExtension(pb::BusinessRuleMessage::message);
            resourceId = businessRuleMessage.businessrule().id();
            break;
        }
        case pb::Message_Type_BroadcastBusinessAction:
        {
            const pb::BroadcastBusinessActionMessage& businessActionMessage = message.GetExtension(pb::BroadcastBusinessActionMessage::message);
            parseBusinessAction(businessAction, businessActionMessage.businessaction());
            break;
        }
        case pb::Message_Type_FileAdd:
        {
            const pb::FileAddMessage& fileAddMessage = message.GetExtension(pb::FileAddMessage::message);
            filename = QString::fromStdString(fileAddMessage.path());
            break;
        }
        case pb::Message_Type_FileRemove:
        {
            const pb::FileRemoveMessage& fileRemoveMessage = message.GetExtension(pb::FileRemoveMessage::message);
            filename = QString::fromStdString(fileRemoveMessage.path());
            break;
        }
        case pb::Message_Type_FileUpdate:
        {
            const pb::FileUpdateMessage& fileUpdateMessage = message.GetExtension(pb::FileUpdateMessage::message);
            filename = QString::fromStdString(fileUpdateMessage.path());
            break;
        }
        case pb::Message_Type_RuntimeInfoChange:
        {
            const pb::RuntimeInfoChangeMessage& runtimeInfoChangeMessage = message.GetExtension(pb::RuntimeInfoChangeMessage::message);
            if (runtimeInfoChangeMessage.has_publicip())
                publicIp = QString::fromStdString(runtimeInfoChangeMessage.publicip());
            if (runtimeInfoChangeMessage.has_systemname())
                systemName = QString::fromStdString(runtimeInfoChangeMessage.systemname());
            if (runtimeInfoChangeMessage.has_sessionkey())
                sessionKey = runtimeInfoChangeMessage.sessionkey().c_str();
            break;
        }
        case pb::Message_Type_BusinessRuleReset:
        {
            const pb::BusinessRuleResetMessage& businessRuleResetMessage = message.GetExtension(pb::BusinessRuleResetMessage::message);
            parseBusinessRules(businessRules, businessRuleResetMessage.businessrule());
            break;
        }
    case pb::Message_Type_KvPairChange:
    {
        const pb::KvPairChangeMessage &kvPairChangeMessage = message.GetExtension(pb::KvPairChangeMessage::message);
        parseKvPairs(kvPairs, kvPairChangeMessage.kvpair());
        break;
    }
    case pb::Message_Type_KvPairDelete:
    {
        const pb::KvPairDeleteMessage &kvPairDeleteMessage = message.GetExtension(pb::KvPairDeleteMessage::message);
        parseKvPairs(kvPairs, kvPairDeleteMessage.kvpair());
        break;
    }

    case pb::Message_Type_Command:
    {
        const pb::CommandMessage &commandMessage = message.GetExtension(pb::CommandMessage::message);
        command = static_cast<Command>(commandMessage.command());
        break;
    }
    case pb::Message_Type_VideoWallControl:
    {
        const pb::VideoWallControlMessage& videoWallControl = message.GetExtension(pb::VideoWallControlMessage::message);
        parseVideoWallControl(videoWallControlMessage, videoWallControl.control());
        break;
    }

    default:
        break;
    }

    return true;
}

quint32 QnMessage::nextSeqNumber(quint32 seqNumber)
{
    seqNumber++;

    if (seqNumber == 0)
        seqNumber++;

    return seqNumber;
}

