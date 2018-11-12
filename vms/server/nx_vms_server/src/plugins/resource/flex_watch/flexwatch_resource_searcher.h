#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource_searcher.h>

namespace nx::mediaserver { class Settings; }

class QnFlexWatchResourceSearcher: public OnvifResourceSearcher
{
public:
    QnFlexWatchResourceSearcher(QnMediaServerModule* serverModule);
    virtual ~QnFlexWatchResourceSearcher();

    // returns all available devices
    virtual QnResourceList findResources() override;
protected:
    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
private:
    bool updateSocketList();
    void clearSocketList();
    void sendBroadcast();
private:
    QList<nx::network::AbstractDatagramSocket*> m_sockList;
    qint64 m_sockUpdateTime;
};

#endif //ENABLE_ONVIF
