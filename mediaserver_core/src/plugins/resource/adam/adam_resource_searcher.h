#pragma once

#ifdef ENABLE_ADVANTECH

#include <memory>
#include <modbus/modbus_client.h>
#include "core/resource_management/resource_searcher.h"
#include "nx/network/socket.h"
#include <qglobal.h>



class QnAdamResourceSearcher : public QnAbstractNetworkResourceSearcher
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
    QnAdamResourceSearcher(QnCommonModule* commonModule);
    ~QnAdamResourceSearcher();

    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck);

    virtual QnResourceList findResources() override;

    virtual QnResourcePtr createResource(
        const QnUuid& resourceTypeId,
        const QnResourceParams& params) override;

    virtual bool isSequential() const override { return true; };

private:
    QString generatePhysicalId(const QString& url) const;
    QByteArray executeAsciiCommand(nx::modbus::QnModbusClient& client, const QString& command);
    QString getAdamModuleName(nx::modbus::QnModbusClient& client);
    QString getAdamModuleFirmware(nx::modbus::QnModbusClient& client);

private:
    std::shared_ptr<nx::modbus::QnModbusClient> m_modbusClient;
};

#endif //< ENABLE_ADVANTECH
