#include "message.h"

#include "message.pb.h"
#include "api/serializer/pb_serializer.h"
#include "api/app_server_connection.h"
#include <core/resource/resource_type.h>

namespace {
    typedef google::protobuf::RepeatedPtrField<pb::Resource>            PbResourceList;
    typedef google::protobuf::RepeatedPtrField<pb::ResourceType>        PbResourceTypeList;
    typedef google::protobuf::RepeatedPtrField<pb::License>             PbLicenseList;
    typedef google::protobuf::RepeatedPtrField<pb::CameraServerItem>    PbCameraServerItemList;
    typedef google::protobuf::RepeatedPtrField<pb::BusinessRule>        PbBusinessRuleList;
}

void parseResource(QnResourcePtr& resource, const pb::Resource& pb_resource, QnResourceFactory& resourceFactory);
void parseLicense(QnLicensePtr& license, const pb::License& pb_license);
void parseCameraServerItem(QnCameraHistoryItemPtr& historyItem, const pb::CameraServerItem& pb_cameraServerItem);
void parseBusinessRule(QnBusinessEventRulePtr& businessRule, const pb::BusinessRule& pb_businessRule);
void parseBusinessAction(QnAbstractBusinessActionPtr& businessAction, const pb::BusinessAction& pb_businessAction);

void parseResourceTypes(QList<QnResourceTypePtr>& resourceTypes, const PbResourceTypeList& pb_resourceTypes);
void parseResources(QnResourceList& resources, const PbResourceList& pb_resources, QnResourceFactory& resourceFactory);
void parseLicenses(QnLicenseList& licenses, const PbLicenseList& pb_licenses);
void parseCameraServerItems(QnCameraHistoryList& cameraServerItems, const PbCameraServerItemList& pb_cameraServerItems);

namespace Qn
{
    QString toString( Message_Type val )
    {
        switch( val )
        {
            case Message_Type_Initial:
                return QLatin1String("Initial");
            case Message_Type_Ping:
                return QLatin1String("Ping");
            case Message_Type_ResourceChange:
                return QLatin1String("ResourceChange");
            case Message_Type_ResourceDelete:
                return QLatin1String("ResourceDelete");
            case Message_Type_ResourceStatusChange:
                return QLatin1String("ResourceStatusChange");
            case Message_Type_ResourceDisabledChange:
                return QLatin1String("ResourceDisabledChange");
            case Message_Type_License:
                return QLatin1String("License");
            case Message_Type_CameraServerItem:
                return QLatin1String("CameraServerItem");
            case Message_Type_BusinessRuleInsertOrUpdate:
                return QLatin1String("BusinessRuleInsertOrUpdate");
            case Message_Type_BusinessRuleDelete:
                return QLatin1String("BusinessRuleDelete");
            case Message_Type_BroadcastBusinessAction:
                return QLatin1String("BroadcastBusinessAction");
            default:
                return QString::fromAscii("Unknown %1").arg((int)val);
        }
    }
}

bool QnMessage::load(const pb::Message &message)
{
    eventType = (Qn::Message_Type)message.type();
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
            parseLicense(license, licenseMessage.license());
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
            licenses.setHardwareId(initialMessage.hardwareid().c_str());
			licenses.setOldHardwareId(initialMessage.oldhardwareid().c_str());
            parseResourceTypes(resourceTypes, initialMessage.resourcetype());
            qnResTypePool->replaceResourceTypeList(resourceTypes);

            parseResources(resources, initialMessage.resource(), *QnAppServerConnectionFactory::defaultFactory());
            parseLicenses(licenses, initialMessage.license());
            parseCameraServerItems(cameraServerItems, initialMessage.cameraserveritem());
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

