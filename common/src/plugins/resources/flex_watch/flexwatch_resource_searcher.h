#ifndef __FLEXWATCH_RESOURCE_SEARCHER_H__
#define __FLEXWATCH_RESOURCE_SEARCHER_H__

#include "../mdns/mdns_device_searcher.h"
#include "plugins/resources/onvif/onvif_resource_searcher.h"


class QnFlexWatchResourceSearcher : public OnvifResourceSearcher
{
    QnFlexWatchResourceSearcher();
    virtual ~QnFlexWatchResourceSearcher();

public:
    static QnFlexWatchResourceSearcher& instance();


    // returns all available devices
    virtual QnResourceList findResources() override;
private:
    bool updateSocketList();
    void clearSocketList();
    void sendBroadcast();
private:
    QList<QUdpSocket*> m_sockList;
    qint64 m_sockUpdateTime;
};

#endif // __FLEXWATCH_RESOURCE_SEARCHER_H__
