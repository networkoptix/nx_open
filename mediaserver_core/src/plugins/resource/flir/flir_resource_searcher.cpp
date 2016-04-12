#include <QtEndian>
#include "flir_resource_searcher.h"
#include "flir_eip_resource.h"

QnFlirResourceSearcher::QnFlirResourceSearcher()
{
    m_eipFlirResTypeId = qnResTypePool->getResourceTypeId(manufacture(), lit("FLIR-AX8"));
    m_cgiFlirResTypeId = qnResTypePool->getResourceTypeId(manufacture(), lit("FLIR_COMMON"));
}
QnFlirResourceSearcher::~QnFlirResourceSearcher()
{
}

QnResourcePtr QnFlirResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;
        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        return result;
    }

    result = QnPhysicalCameraResourcePtr( new QnFlirEIPResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create FLIR camera resource. typeID:" << resourceTypeId.toString();

    return result;

}

QList<QnResourcePtr> QnFlirResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    QList<QnResourcePtr> result;
    FlirDeviceInfo deviceInfo;
    auto hostname = url.host().isEmpty() ?
        QHostAddress(url.toString()): QHostAddress(url.host());
    SimpleEIPClient eipClient(hostname);

    if(!url.scheme().isEmpty() && doMultichannelCheck)
        return result;

    if(!eipClient.connect())
    {
        qDebug() << "FLIR checkHostAddr Failed to establish connection" << "EIP HOST" << url.host();
        return  result;
    }
    if(!eipClient.registerSession())
    {
        qDebug() << "FLIR checkHostAddr failed to register session";
        return result;
    }


    const auto vendorId = getVendorIdFromDevice(eipClient);
    if(vendorId != FLIR_VENDOR_ID)
    {
        return result;
    }

    deviceInfo.model = getModelFromDevice(eipClient);
    deviceInfo.mac = getMACAdressFromDevice(eipClient);
    deviceInfo.firmware = getFirmwareFromDevice(eipClient);
    deviceInfo.url = url;
    deviceInfo.url.setScheme("http");

    createResource(deviceInfo, auth, result);

    return result;
}

quint16 QnFlirResourceSearcher::getVendorIdFromDevice(SimpleEIPClient& eipClient) const
{
    auto data = eipClient.doServiceRequest(
        CIPServiceCode::CIP_SERVICE_GET_ATTRIBUTE_SINGLE,
        FlirEIPClass::IDENTITY,
        0x01,
        FlirEIPIdentityAttribute::VENDOR_NUMBER,
        QByteArray());

    if(data.generalStatus != CIPGeneralStatus::SUCCESS &&
        data.generalStatus != CIPGeneralStatus::ALREADY_IN_REQUESTED_MODE)
    {
        qDebug() << "ERROR OCCURED WHEN RETRIEVING VENDOR" << data.generalStatus << data.additionalStatus;
    }

    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data.data.constData()));
}

QString QnFlirResourceSearcher::getMACAdressFromDevice(SimpleEIPClient& eipClient) const
{
    auto data = eipClient.doServiceRequest(
        CIPServiceCode::CIP_SERVICE_GET_ATTRIBUTE_SINGLE,
        FlirEIPClass::ETHERNET,
        0x01,
        FlirEIPEthernetAttribute::PHYSICAL_ADDRESS,
        QByteArray());

    if(data.generalStatus != CIPGeneralStatus::SUCCESS &&
        data.generalStatus != CIPGeneralStatus::ALREADY_IN_REQUESTED_MODE)
    {
        qDebug() << "ERROR OCCURED WHEN RETRIEVING MAC ADDRESS" << data.generalStatus << data.additionalStatus;
    }

    return QString(data.data.toHex());
}

QString QnFlirResourceSearcher::getModelFromDevice(SimpleEIPClient& eipClient) const
{
    auto data = eipClient.doServiceRequest(
        CIPServiceCode::CIP_SERVICE_GET_ATTRIBUTE_SINGLE,
        FlirEIPClass::IDENTITY,
        0x01,
        FlirEIPIdentityAttribute::PRODUCT_NAME,
        QByteArray());

    if(data.generalStatus != CIPGeneralStatus::SUCCESS &&
        data.generalStatus != CIPGeneralStatus::ALREADY_IN_REQUESTED_MODE)
    {
        qDebug() << "ERROR OCCURED WHEN RETRIEVING MODEL" << data.generalStatus << data.additionalStatus;
    }

    auto model = QString::fromLatin1(data.data.mid(1));
    if(model.toLower().startsWith("flir") && model.indexOf(" ") != -1)
        model = model.split(" ")[1];
    return model;
}

QString QnFlirResourceSearcher::getFirmwareFromDevice(SimpleEIPClient &eipClient) const
{
    QString requestParam(".version.kits.appkit.ver");

    QByteArray requestData;
    requestData[0] = requestParam.size();
    requestData.append(requestParam.toLatin1());

    auto data = eipClient.doServiceRequest(
        FlirEIPPassthroughService::READ_ASCII,
        FlirEIPClass::PASSTHROUGH,
        0x01,
        0,
        requestData);

    return QString(QString::fromLatin1(data.data.mid(1)));
}

QString QnFlirResourceSearcher::manufacture() const
{
    return QnFlirEIPResource::MANUFACTURE;
}

QList<QnNetworkResourcePtr> QnFlirResourceSearcher::processPacket(
    QnResourceList &result,
    const QByteArray &responseData,
    const QHostAddress &discoveryAddress,
    const QHostAddress &foundHostAddress)
{
    QList<QnNetworkResourcePtr> localRes;

    return localRes;
}


void QnFlirResourceSearcher::createResource(const FlirDeviceInfo& info, const QAuthenticator& auth, QList<QnResourcePtr>& result)
{
    QnFlirEIPResourcePtr resource(new QnFlirEIPResource());

    resource->setName(manufacture() + lit("-") + info.model);
    resource->setModel(info.model);
    resource->setVendor(manufacture());
    resource->setTypeId(m_eipFlirResTypeId);
    resource->setUrl(info.url.toString());
    resource->setMAC(QnMacAddress(info.mac));
    resource->setDefaultAuth(auth);
    resource->setFirmware(info.firmware);

    result << resource;
}

