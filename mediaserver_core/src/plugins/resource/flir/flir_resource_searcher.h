#pragma once

#ifdef ENABLE_FLIR

#include "../mdns/mdns_resource_searcher.h"
#include "flir_eip_data.h"
#include "simple_eip_client.h"

struct FlirDeviceInfo
{
    QString mac;
    QString model;
    QString firmware;
    nx::utils::Url url;
};

class QnFlirResourceSearcher: public QnMdnsResourceSearcher
{
    using base_type = QnMdnsResourceSearcher;
public:
    QnFlirResourceSearcher(QnCommonModule* commonModule);
    ~QnFlirResourceSearcher();

    static const quint16 kFlirVendorId = 1161;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;
    virtual QString manufacture() const override;

protected:
    virtual QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& result,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress ) override;

private:
    QnUuid m_eipFlirResTypeId;
    QnUuid m_cgiFlirResTypeId;

    QMap<QString, QString> m_sessionMapByHostname;

    void createResource(const FlirDeviceInfo& info, const QAuthenticator& auth, QList<QnResourcePtr>& result);

    QString getMACAdressFromDevice(SimpleEIPClient& eipClient) const;
    QString getModelFromDevice(SimpleEIPClient& eipClient) const;
    quint16 getVendorIdFromDevice(SimpleEIPClient& eipClient) const;
    QString getFirmwareFromDevice(SimpleEIPClient& eipClient) const;

};

#endif // #ifdef ENABLE_FLIR
