#ifndef __FLEXWATCH_RESOURCE_SEARCHER_H__
#define __FLEXWATCH_RESOURCE_SEARCHER_H__

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource_searcher.h>


class QnFlexWatchResourceSearcher: public OnvifResourceSearcher
{
public:
    QnFlexWatchResourceSearcher(QnCommonModule* commonModule);
    virtual ~QnFlexWatchResourceSearcher();

    // returns all available devices
    virtual QnResourceList findResources() override;

protected:
    virtual QnResourceList checkEndpoint(
        const QUrl& url, const QAuthenticator& auth,
        const QString& physicalId, QnResouceSearchMode mode) override;

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
