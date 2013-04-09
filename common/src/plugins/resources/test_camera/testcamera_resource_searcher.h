#ifndef __TEST_CAMERA_RESOURCE_SEARCHER_H_
#define __TEST_CAMERA_RESOURCE_SEARCHER_H_

#include "core/resource_managment/resource_searcher.h"
#include "utils/network/socket.h"

class QnTestCameraResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    Q_DECLARE_TR_FUNCTIONS(QnTestCameraResourceSearcher)
    QnTestCameraResourceSearcher();
    virtual ~QnTestCameraResourceSearcher();
public:
    static QnTestCameraResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
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
        DiscoveryInfo(QUdpSocket* _sock, const QHostAddress& _ifAddr): sock(_sock), ifAddr(_ifAddr) {}
        ~DiscoveryInfo() { }
        QUdpSocket* sock;
        QHostAddress ifAddr;
    };
    QList<DiscoveryInfo> m_sockList;
    qint64 m_sockUpdateTime;
};

#endif // __TEST_CAMERA_RESOURCE_SEARCHER_H_
