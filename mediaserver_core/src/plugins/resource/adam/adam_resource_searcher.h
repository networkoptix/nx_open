#pragma once

#include <memory>
#include <modbus/modbus_client.h>
#include "core/resource_management/resource_searcher.h"
#include "utils/network/socket.h"
#include <qglobal.h>

struct QnAdamAsciiCommand
{
    QnAdamAsciiCommand(const QString& string);
    static const quint16 kStartAsciiRegister= 9999;
    static const quint16 kReadAsciiRegisterCount = 125;
    QByteArray data;
    size_t wordNum;
    size_t byteNum;
};

class QnAdamResourceSearcher : public QnAbstractNetworkResourceSearcher
{
public:
    QnAdamResourceSearcher();
    ~QnAdamResourceSearcher();

    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const QUrl& url, 
        const QAuthenticator& auth, 
        bool doMultichannelCheck);

    virtual QnResourceList findResources() override;

    virtual QnResourcePtr createResource(
        const QnUuid& resourceTypeId, 
        const QnResourceParams& params) override;

private:
    QString generatePhysicalId(const QString& url) const;
    QByteArray executeAsciiCommand(nx_modbus::QnModbusClient& client, const QString& command);
    QString getAdamModuleName(nx_modbus::QnModbusClient& client);
    QString getAdamModuleFirmware(nx_modbus::QnModbusClient& client);

private:
    QnUuid m_typeId;
    std::shared_ptr<nx_modbus::QnModbusClient> m_modbusClient;
    std::shared_ptr<UDPSocket> m_broadcastSocket;
};
