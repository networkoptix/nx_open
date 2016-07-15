#include "adam_resource.h"
#include "adam_resource_searcher.h"
#include <modbus/modbus_client.h>
#include <common/common_module.h>
#include <utils/common/log.h>

namespace
{
    const QString kDefaultUser("root");
    const QString kDefaultPassword("00000000");
    const QString kAdamResourceType("ADVANTECH_ADAM_6000");
    const QString kHashPrefix("ADVANTECH_ADAM_MODULE_");
}

QnAdamAsciiCommand::QnAdamAsciiCommand(const QString& command)
{
    data = command.toLatin1();
    data.append("\r");
    data.append((char)0);

    byteNum = data.size();
    wordNum = byteNum / 2;
}

QnAdamResourceSearcher::QnAdamResourceSearcher()
{
    m_typeId = qnResTypePool->getResourceTypeId(manufacture(), kAdamResourceType);
}

QnAdamResourceSearcher::~QnAdamResourceSearcher()
{

}

QString QnAdamResourceSearcher::manufacture() const
{
    return QnAdamResource::kManufacture;
}

QString QnAdamResourceSearcher::generatePhysicalId(const QString& url) const
{
    const auto kPrefixedUrl = kHashPrefix + url;
    auto hash = QCryptographicHash::hash(kPrefixedUrl.toUtf8(), QCryptographicHash::Md5 ).toHex();

    return QString::fromUtf8(hash);
}

QByteArray QnAdamResourceSearcher::executeAsciiCommand(
    nx_modbus::QnModbusClient& client, 
    const QString& commandStr)
{
    QnAdamAsciiCommand command(commandStr);

    auto response = client.writeHoldingRegisters(
        QnAdamAsciiCommand::kStartAsciiRegister,
        command.data);

    //TODO check response for validity

    response = client.readHoldingRegisters(
        QnAdamAsciiCommand::kStartAsciiRegister,
        QnAdamAsciiCommand::kReadAsciiRegisterCount);

    //TODO check response for validity

    qDebug() << "Index of exclamation mark" << response.data.indexOf("!");
    if(response.data.indexOf("!") != 1)
        return QByteArray();

    auto endOfStr = response.data.indexOf("\r");

    return response.data.mid(4, endOfStr);
}

QString QnAdamResourceSearcher::getAdamModuleName(nx_modbus::QnModbusClient& client)
{
    QString commandStr("$01M");

    auto response = executeAsciiCommand(client, commandStr);

    if(response.isEmpty())
        return QString();

    return QString::fromLatin1(response).trimmed();
}

QString QnAdamResourceSearcher::getAdamModuleFirmware(nx_modbus::QnModbusClient& client)
{
    QString commandStr("$01F");

    auto response = executeAsciiCommand(client, commandStr);

    if(response.isEmpty())
        return QString();

    return QString::fromLatin1(response).trimmed();
}

QList<QnResourcePtr> QnAdamResourceSearcher::checkHostAddr(
    const QUrl &url, 
    const QAuthenticator &auth, 
    bool doMultichannelCheck)
{

    qDebug() << "Checking url for Advantech ADAM module:" << url;

    QList<QnResourcePtr> result;
    if( !url.scheme().isEmpty() && doMultichannelCheck )
        return result;

    SocketAddress endpoint(url.host(), 502);

    nx_modbus::QnModbusClient modbusClient(endpoint);

    qDebug() << "Connecting to endpoint";

    if(!modbusClient.connect())
        return result;

    auto moduleName = getAdamModuleName(modbusClient);

    qDebug() << "Got module name from device:" << moduleName;

    if(moduleName.isEmpty())
        return result;

    auto firmware = getAdamModuleFirmware(modbusClient);

    qDebug() << "Got firmware from device:" << firmware;

    if(firmware.isEmpty())
        return result;

    auto model = lit("ADAM-") + moduleName;

    qDebug() << "Advantech model" << model;

    QnUuid typeId = qnResTypePool->getResourceTypeId(lit("AdvantechADAM"), model);

    if (typeId.isNull())
        return QList<QnResourcePtr>();

    QnAdamResourcePtr resource(new QnAdamResource());
    resource->setTypeId(typeId);
    resource->setName(model);
    resource->setModel(model);
    resource->setFirmware(firmware);

    // Advantech ADAM modules do not have any unique identifier that we can obtain.
    qDebug() << "PhysicalId" << generatePhysicalId(url.toString());

    auto uid = generatePhysicalId(url.toString());

    modbusClient.disconnect();

    resource->setPhysicalId(uid);

    QUrl webInterfaceUrl(url);
    webInterfaceUrl.setScheme(lit("http"));
    resource->setVendor(lit("Advantech"));
    resource->setMAC( QnMacAddress(uid));
    resource->setUrl(webInterfaceUrl.toString());
    resource->setDefaultAuth(auth);

    result << resource;

    return result;
}

QnResourceList QnAdamResourceSearcher::findResources()
{
    QnResourceList result;
    // TODO: #dmishin Need to reverse their discovery protocol.

    return result;
}

QnResourcePtr QnAdamResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams &params)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        NX_LOG(lit("No resource type for ID %1").arg(resourceTypeId.toString()), cl_logDEBUG1);
        return result;
    }

    if (resourceType->getManufacture() != manufacture())
        return result;

    result.reset(new QnAdamResource());
    result->setTypeId(resourceTypeId);

    NX_LOG(lit("Create Advantech ADAM-6000 series camera resource. TypeID %1.").arg(resourceTypeId.toString()), cl_logDEBUG1);
    return result;
}
