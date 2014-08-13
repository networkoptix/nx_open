#ifndef __TEST_CAMERA_RESOURCE_SEARCHER_H_
#define __TEST_CAMERA_RESOURCE_SEARCHER_H_

#ifdef ENABLE_TEST_CAMERA

#include <QtCore/QCoreApplication>

#include "core/resource_management/resource_searcher.h"
#include "utils/network/socket.h"

class QnTestCameraResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    Q_DECLARE_TR_FUNCTIONS(QnTestCameraResourceSearcher)
public:
    QnTestCameraResourceSearcher();
    virtual ~QnTestCameraResourceSearcher();

    QnResourceList findResources(void);

    virtual QnResourcePtr createResource(const QUuid &resourceTypeId, const QnResourceParams& params) override;
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

private:

    void sendBroadcast();
    bool updateSocketList();
    void clearSocketList();

private:
    struct DiscoveryInfo
    {
        DiscoveryInfo( AbstractDatagramSocket* _sock, const QHostAddress& _ifAddr): sock(_sock), ifAddr(_ifAddr) {}
        ~DiscoveryInfo() { }
        AbstractDatagramSocket* sock;
        QHostAddress ifAddr;
    };
    QList<DiscoveryInfo> m_sockList;
    qint64 m_sockUpdateTime;
};

#endif // #ifdef ENABLE_TEST_CAMERA
#endif // __TEST_CAMERA_RESOURCE_SEARCHER_H_
