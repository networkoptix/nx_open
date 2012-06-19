#include "message.h"

#include "message.pb.h"
#include "api/serializer/pb_serializer.h"
#include "api/app_server_connection.h"

void parseResource(QnResourcePtr& resource, const pb::Resource& pb_resource, QnResourceFactory& resourceFactory);
void parseLicense(QnLicensePtr& license, const pb::License& pb_license);
void parseCameraServerItem(QnCameraHistoryItemPtr& historyItem, const pb::CameraServerItem& pb_cameraServerItem);

bool QnMessage::load(const pb::Message &message)
{
    eventType = (Message_Type)message.type();
    seqNumber = message.seqnumber();

    switch (eventType)
    {
        case pb::Message_Type_ResourceChange:
        case pb::Message_Type_ResourceDisabledChange:
        case pb::Message_Type_ResourceStatusChange:
        {
			const pb::ResourceMessage& resourceMessage = message.GetExtension(pb::ResourceMessage::message);
			parseResource(resource, resourceMessage.resource(), *QnAppServerConnectionFactory::defaultFactory());
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
        case pb::Message_Type_ResourceDelete:
        {
			const pb::ResourceMessage& resourceMessage = message.GetExtension(pb::ResourceMessage::message);

			// TODO: Ivan
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

