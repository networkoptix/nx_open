#ifndef __FLEXWATCH_RESOURCE_SEARCHER_H__
#define __FLEXWATCH_RESOURCE_SEARCHER_H__

#ifdef ENABLE_ONVIF

#include "../mdns/mdns_device_searcher.h"
#include "plugins/resource/onvif/onvif_resource_searcher.h"


class QnFlexWatchResourceSearcher : public OnvifResourceSearcher
{
public:
    QnFlexWatchResourceSearcher();
    virtual ~QnFlexWatchResourceSearcher();

    // returns all available devices
    virtual QnResourceList findResources() override;
protected:
    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
private:
    bool updateSocketList();
    void clearSocketList();
    void sendBroadcast();
private:
    QList<AbstractDatagramSocket*> m_sockList;
    qint64 m_sockUpdateTime;
};

#endif //ENABLE_ONVIF

#endif // __FLEXWATCH_RESOURCE_SEARCHER_H__
