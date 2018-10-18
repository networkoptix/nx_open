#pragma once

#ifdef ENABLE_MDNS

#include <core/resource/network_resource.h>
#include <core/resource_management/resource_searcher.h>

class QnMediaServerModule;

class QnMdnsResourceSearcher: virtual public QnAbstractNetworkResourceSearcher
{
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnMdnsResourceSearcher(QnMediaServerModule* serverModule);
    ~QnMdnsResourceSearcher();

    bool isProxy() const;

    virtual QnResourceList findResources() override;

protected:
    /*!
        \param result Just found resources. In case if same camera has been found on multiple network interfaces
        \param Local address, MDNS response had been recevied on
        \param foundHostAddress Source address of received MDNS packet
        \note Searcher MUST not duplicate resoures, already present in \a result
    */
    virtual QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& result,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress ) = 0;
private:
    QnMediaServerModule* m_serverModule = nullptr;
};

#endif // ENABLE_MDNS
