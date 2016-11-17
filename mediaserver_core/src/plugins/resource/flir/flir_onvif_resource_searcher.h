#pragma once

#include <plugins/resource/onvif/onvif_resource_searcher.h>

class FlirOnvifResourceSearcher: public OnvifResourceSearcher
{
    virtual QList<QnResourcePtr> checkHostAddr(
        const QUrl& url, 
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    virtual QnResourceList findResources() override;

    virtual bool isSequential() const override;
};