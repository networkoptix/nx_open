#pragma once
#if defined(ENABLE_TEST_CAMERA)

#include <set>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QCoreApplication>
#include <QtNetwork/QHostAddress>

#include <core/resource_management/resource_searcher.h>
#include <nx/network/socket.h>
#include <nx/utils/mac_address.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/resource/resource_fwd.h>

class QnMediaServerModule;

class QnTestCameraResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_DECLARE_TR_FUNCTIONS(QnTestCameraResourceSearcher)

public:
    QnTestCameraResourceSearcher(QnMediaServerModule* serverModule);
    virtual ~QnTestCameraResourceSearcher();

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacturer() const override;

    virtual QnResourceList findResources() override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

private:
    void sendBroadcast();
    bool updateSocketListIfNeeded();

    void processDiscoveryResponseMessage(
        const QByteArray& discoveryResponseMessage,
        const QString& testcameraAddress,
        QnResourceList* resources,
        std::set<nx::utils::MacAddress>* processedMacAddresses) const;

    QnTestCameraResourcePtr createDiscoveredTestCameraResource(
        const nx::utils::MacAddress& macAddress,
        const QString& videoLayoutString,
        int mediaPort,
        const QString& testcameraAddress) const;

    bool readDiscoveryResponse(
        nx::network::AbstractDatagramSocket* socket,
        QnResourceList* resources,
        std::set<nx::utils::MacAddress>* processedMacAddresses) const;

private:
    std::vector<std::unique_ptr<nx::network::AbstractDatagramSocket>> m_discoverySockets;
    qint64 m_socketUpdateTime = 0;
};

#endif // defined(ENABLE_TEST_CAMERA)
