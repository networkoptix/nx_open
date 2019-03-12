#pragma once

#ifdef ENABLE_ADVANTECH

#include <memory>
#include <modbus/modbus_client.h>
#include "core/resource_management/resource_searcher.h"
#include "nx/network/socket.h"
#include <qglobal.h>
#include <nx/vms/server/server_module_aware.h>

class QnAdamResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    struct QnAdamAsciiCommand
    {
        QnAdamAsciiCommand(const QString& string);
        static const quint16 kStartAsciiRegister= 9999;
        static const quint16 kReadAsciiRegisterCount = 125;
        QByteArray data;
        size_t wordNum;
        size_t byteNum;
    };

public:
    QnAdamResourceSearcher(QnMediaServerModule* serverModule);
    ~QnAdamResourceSearcher();

    virtual QnResourcePtr createResource(
        const QnUuid& resourceTypeId,
        const QnResourceParams& params) override;

    virtual bool isSequential() const override { return true; }

    virtual QString manufacturer() const override;

    virtual QnResourceList findResources() override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

private:
    QString generatePhysicalId(const QString& url) const;
    QString executeAsciiCommand(nx::modbus::QnModbusClient& client, const QString& command);
    QString getAdamModel(nx::modbus::QnModbusClient& client);
    QString getAdamModuleFirmware(nx::modbus::QnModbusClient& client);

private:
    std::shared_ptr<nx::modbus::QnModbusClient> m_modbusClient;
};

#endif //< ENABLE_ADVANTECH
