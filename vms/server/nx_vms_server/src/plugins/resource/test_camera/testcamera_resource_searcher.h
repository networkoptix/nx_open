#pragma once

#ifdef ENABLE_TEST_CAMERA

#include <QtCore/QCoreApplication>
#include <QHostAddress>

#include "core/resource_management/resource_searcher.h"
#include <nx/network/socket.h>
#include <nx/vms/server/server_module_aware.h>

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
    bool updateSocketList();
    void clearSocketList();

private:
    struct DiscoveryInfo
    {
        DiscoveryInfo( nx::network::AbstractDatagramSocket* _sock, const QHostAddress& _ifAddr): sock(_sock), ifAddr(_ifAddr) {}
        ~DiscoveryInfo() { }
        nx::network::AbstractDatagramSocket* sock;
        QHostAddress ifAddr;
    };
    QList<DiscoveryInfo> m_sockList;
    qint64 m_sockUpdateTime = 0;
};

#endif // #ifdef ENABLE_TEST_CAMERA
