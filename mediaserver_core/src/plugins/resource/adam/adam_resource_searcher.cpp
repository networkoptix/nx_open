#ifdef ENABLE_ADVANTECH

#include "adam_resource.h"
#include "adam_resource_searcher.h"

#include <modbus/modbus_client.h>
#include <common/common_module.h>
#include <nx/utils/log/log.h>
#include <utils/common/sleep.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/system_socket.h>

namespace
{
    const QString kDefaultUser("root");
    const QString kDefaultPassword("00000000");
    const QString kAdamResourceType("AdvantechADAM");
    const QString kHashPrefix("ADVANTECH_ADAM_MODULE_");

    /**
     * I have no idea what it means but such a message is used by
     * Advantech Adam/Apax .NET Utility.
     */
    const unsigned char kAdamSearchMessage[60] =
    {
        0x4d, 0x41, 0x44, 0x41, 0x00, 0x00, 0x00, 0x83, 0x01, 0x00,
        0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    const quint16 kAdamAutodiscoveryPort = 5048;
    const quint32 kAdamBroadcastBufferSize = 1024 * 5;
    const quint32 kAdamBroadcastIntervalMs = 10000;
    const quint32 kAdamBroadcastSleepInterval = 500;
}

QnAdamResourceSearcher::QnAdamAsciiCommand::QnAdamAsciiCommand(const QString& command)
{
    data = command.toLatin1();
    data.append("\r");
    data.append((char)0);

    byteNum = data.size();
    wordNum = byteNum / 2;
}

QnAdamResourceSearcher::QnAdamResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
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
    nx::modbus::QnModbusClient& client,
    const QString& commandStr)
{
    QnAdamAsciiCommand command(commandStr);
    bool status = false;

    auto response = client.writeHoldingRegisters(
        QnAdamAsciiCommand::kStartAsciiRegister,
        command.data,
        &status);

    if (response.isException() || !status)
        return QByteArray();

    response = client.readHoldingRegisters(
        QnAdamAsciiCommand::kStartAsciiRegister,
        QnAdamAsciiCommand::kReadAsciiRegisterCount,
        &status);

    if (response.isException() || !status)
        return QByteArray();

    if(response.data.indexOf("!") != 1)
        return QByteArray();

    auto endOfStr = response.data.indexOf("\r");

    return response.data.mid(4, endOfStr);
}

QString QnAdamResourceSearcher::getAdamModuleName(nx::modbus::QnModbusClient& client)
{
    QString commandStr("$01M");

    auto response = executeAsciiCommand(client, commandStr);

    if(response.isEmpty())
        return QString();

    return QString::fromLatin1(response).trimmed();
}

QString QnAdamResourceSearcher::getAdamModuleFirmware(nx::modbus::QnModbusClient& client)
{
    QString commandStr("$01F");

    auto response = executeAsciiCommand(client, commandStr);

    if(response.isEmpty())
        return QString();

    return QString::fromLatin1(response).trimmed();
}

QList<QnResourcePtr> QnAdamResourceSearcher::checkHostAddr(const nx::utils::Url &url,
    const QAuthenticator &auth,
    bool doMultichannelCheck)
{
    QList<QnResourcePtr> result;
    if( !url.scheme().isEmpty() && doMultichannelCheck )
        return result;

    nx::network::SocketAddress endpoint(url.host(), url.port(nx::modbus::kDefaultModbusPort));

    nx::modbus::QnModbusClient modbusClient(endpoint);

    if(!modbusClient.connect())
        return result;

    auto moduleName = getAdamModuleName(modbusClient);

    if(moduleName.isEmpty())
        return result;

    auto firmware = getAdamModuleFirmware(modbusClient);

    if(firmware.isEmpty())
        return result;

    auto model = lit("ADAM-") + moduleName;

    QnUuid typeId = qnResTypePool->getResourceTypeId(kAdamResourceType, model);

    if (typeId.isNull())
        return QList<QnResourcePtr>();

    QnAdamResourcePtr resource(new QnAdamResource());
    resource->setTypeId(typeId);
    resource->setName(model);
    resource->setModel(model);
    resource->setFirmware(firmware);

    modbusClient.disconnect();

    nx::utils::Url modbusUrl(url);
    modbusUrl.setScheme(lit("http"));
    modbusUrl.setPort(url.port(nx::modbus::kDefaultModbusPort));
    resource->setVendor(lit("Advantech"));
    resource->setUrl(modbusUrl.toString());

    // Advantech ADAM modules do not have any unique identifier that we can obtain.
    auto uid = generatePhysicalId(modbusUrl.toString());
    resource->setPhysicalId(uid);
    resource->setMAC( nx::network::QnMacAddress(uid));

    resource->setDefaultAuth(auth);

    result << resource;

    return result;
}

QnResourceList QnAdamResourceSearcher::findResources()
{
    QnResourceList result;
    auto interfaces = nx::network::getAllIPv4Interfaces();

    for (const auto& iface: interfaces)
    {
        if (shouldStop())
            return result;

        auto socket = std::make_shared<nx::network::UDPSocket>(AF_INET);
        socket->setReuseAddrFlag(true);

        nx::network::SocketAddress localAddress(iface.address.toString(), 0);
        if (!socket->bind(localAddress))
        {
            qDebug() << "Unable to bind socket to local address" << localAddress.toString();
            continue;
        }

        nx::network::SocketAddress foreignEndpoint(nx::network::BROADCAST_ADDRESS, kAdamAutodiscoveryPort);
        if (!socket->sendTo(kAdamSearchMessage, sizeof(kAdamSearchMessage), foreignEndpoint))
        {
            qDebug() << "Unable to send data to " << foreignEndpoint.toString();
            continue;
        }

        QnSleep::msleep(kAdamBroadcastSleepInterval);

        while (socket->hasData())
        {
            QByteArray datagram;
            datagram.resize(nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

            nx::network::SocketAddress remoteEndpoint;
            auto bytesRead = socket->recvFrom(datagram.data(), datagram.size(), &remoteEndpoint);

            if (bytesRead <= 0)
                return result;

            if (remoteEndpoint.port != kAdamAutodiscoveryPort)
                continue;

            auto existingResources = resourcePool()->getAllNetResourceByHostAddress(remoteEndpoint.address.toString());

            if (!existingResources.isEmpty())
            {
                for (const auto& existingRes: existingResources)
                {
                    auto secRes = existingRes.dynamicCast<QnSecurityCamResource>();
                    if (!secRes)
                        continue;

                    QnUuid typeId = qnResTypePool->getResourceTypeId(
                        lit("AdvantechADAM"),
                        secRes->getModel());

                    if (typeId.isNull())
                        continue;

                    QnAdamResourcePtr resource(new QnAdamResource());
                    resource->setTypeId(typeId);
                    resource->setName(secRes->getModel());
                    resource->setModel(secRes->getName());
                    resource->setFirmware(secRes->getFirmware());
                    resource->setPhysicalId(secRes->getPhysicalId());
                    resource->setVendor(secRes->getVendor());
                    resource->setMAC( nx::network::QnMacAddress(secRes->getPhysicalId()));
                    resource->setUrl(secRes->getUrl());
                    resource->setAuth(secRes->getAuth());

                    result.push_back(resource);

                }
                continue;
            }
            else
            {
                nx::utils::Url url;
                url.setScheme(lit("http"));
                url.setHost(remoteEndpoint.address.toString());
                url.setPort(nx::modbus::kDefaultModbusPort);

                // No user/password required.
                QAuthenticator auth;

                auto res = checkHostAddr(url, auth, false);

                result.append(res);
            }
        }
    }

    return result;
}

QnResourcePtr QnAdamResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams &params)
{
    QN_UNUSED(params);

    QnNetworkResourcePtr result;
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for for ID" << resourceTypeId;
        NX_LOG(lit("No resource type for ID %1").arg(resourceTypeId.toString()), cl_logDEBUG1);
        return result;
    }

    if (resourceType->getManufacture() != manufacture())
        return result;

    result.reset(new QnAdamResource());
    result->setTypeId(resourceTypeId);

    NX_LOG(lit("Create Advantech ADAM-6000 series IO module resource. TypeID %1.")
        .arg(resourceTypeId.toString()),
        cl_logDEBUG1);
    return result;
}

#endif //< ENABLE_ADVANTECH
