#pragma once

#include <plugins/resource/onvif/onvif_resource_searcher.h>

namespace nx {
namespace plugins {
namespace flir {

class OnvifResourceSearcher: public ::OnvifResourceSearcher
{
    virtual QList<QnResourcePtr> checkHostAddr(
        const QUrl& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    virtual QnResourceList findResources() override;

    virtual bool isSequential() const override;
};

} // namespace flir
} // namespace plugins
} // namespace nx