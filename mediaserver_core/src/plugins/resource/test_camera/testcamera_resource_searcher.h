#pragma once

#ifdef ENABLE_TEST_CAMERA

#include <QtCore/QCoreApplication>

#include "core/resource_management/resource_searcher.h"
#include <nx/network/socket.h>

class QnMediaServerModule;

class QnTestCameraResourceSearcher: public QnAbstractNetworkResourceSearcher
{
    Q_DECLARE_TR_FUNCTIONS(QnTestCameraResourceSearcher)
public:
    QnTestCameraResourceSearcher(QnMediaServerModule* serverModule);
    virtual ~QnTestCameraResourceSearcher();

    QnResourceList findResources(void);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

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
    qint64 m_sockUpdateTime;
    QnMediaServerModule* m_serverModule = nullptr;
};

#endif // #ifdef ENABLE_TEST_CAMERA
