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
}

QnAdamAsciiCommand::QnAdamAsciiCommand(const QString& command)
{
    data = command.toLatin1();
    data.append("\r\0");
    byteNum = commandBytes.size();
    wordNum = byteNum / 2;
}


QnAdamResourceSearcher::QnAdamResourceSearcher()
{
    m_typeId = qnResTypePool->getResourceTypeId(manufacture(), kAdamResourceType);
}

QnAdamResource::~QnAdamResource()
{
}

QString QnAdamResourceSearcher::manufacture()
{
    return QnAdamResource::kManufacture;
}

QByteArray QnAdamResourceSearcher::executeAsciiCommand(QnModbusClient &client, const QString &commandStr)
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

    if(!response.data.startsWith("!"))
        return QByteArray();

    return response.data.mid(5);
}

QString QnAdamResourceSearcher::getAdamModuleName(QnModbusClient &client)
{
    QString commandStr("$01M");

    auto res = executeAsciiCommand(client, commandStr);

    if(res.isEmpty())
        return QString();

    auto endOfStr = response.data.indexOf('\0');

    return QString::fromLatin1(response.data.left(endOfStr));
}

QString QnAdamResourceSearcher::getAdamModuleFirmawre(QnModbusClient &client)
{
    QString commandStr("$01F");

    auto res = executeAsciiCommand(client, commandStr);

    if(res.isEmpty())
        return QString();

    auto endOfStr = response.data.indexOf('\0');

    return QString::fromLatin1(response.data.left(endOfStr));
}

QList<QnResourcePtr> QnAdamResourceSearcher::checkHostAddr(const QUrl &url, const QAuthenticator &auth, bool doMultichannelCheck)
{

    QList<QnResourcePtr> result;
    if( !url.scheme().isEmpty() && isSearchAction )
        return result;

    SocketAddress endpoint(url.host(), url.port());
    if(!m_modbusClient.connect(endpoint))
        return result;

    auto moduleName = getAdamModuleName(m_modbusClient);

    if(moduleName.isEmpty())
        return result;

    auto firmwareVer = getAdamModuleFirmware(m_modbusClient);

    if(firmwareVer.isEmpty())
        return result;

    auto model = lit("ADAM-") + moduleName;

    QnUuid typeId = qnResTypePool->getLikeResourceTypeId(manufacture(), name);
    if (typeId.isNull())
        return QList<QnResourcePtr>();

    QnAdamResourcePtr resource(new QnAdamResource());
    resource->setTypeId(m_typeId);
    resource->setName(model);
    resource->setModel(model);

    //What should I do with this??
    //resource->setMAC(QnMacAddress(mac));
    //resource->setUniqId();
    //respurce->setPhysicalId();


    QUrl finalUrl(url);
    finalUrl.setScheme(lit("//"));
    resource->setUrl(finalUrl.toString());
    resource->setDefaultAuth(auth);

    result << resource;

    return result;
}

QnResourceList QnAdamResourceSearcher::findResources()
{
    //Need to reverse their discovery protocol
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

    result = QnResourcePtr( new QnAdamResource() );
    result->setTypeId(resourceTypeId);

    NX_LOG(lit("Create Advantech ADAM-6000 series camera resource. TypeID %1.").arg(resourceTypeId.toString()), cl_logDEBUG1);
    return result;
}
