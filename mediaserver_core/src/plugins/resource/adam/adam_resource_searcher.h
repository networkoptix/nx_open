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
    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck);
    virtual QnResourceList findResources() override;

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;

private:
    QString executeAsciiCommand(QnModbusClient& client, const QString& command);
    QString getAdamModuleName(QnModbusClient& client);
    QString getAdamModuleFirmware(QnModbusClient& client);

private:
    QnUuid m_typeId;
    std::shared_ptr<QnModbusClient> m_modbusClient;
    std::shared_ptr<UDPSocket> m_broadcastSocket;
};
