#include "message.h"

#include "message.pb.h"
#include "api/serializer/pb_serializer.h"
#include "api/app_server_connection.h"

void parseResource(QnResourcePtr& resource, const pb::Resource& pb_resource, QnResourceFactory& resourceFactory);
void parseLicense(QnLicensePtr& license, const pb::License& pb_license);
void parseCameraServerItem(QnCameraHistoryItemPtr& historyItem, const pb::CameraServerItem& pb_cameraServerItem);

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
            break;
        case pb::Message_Type_Ping:
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

