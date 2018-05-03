#pragma once

#if defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)

#include <plugins/resource/onvif/onvif_resource_searcher.h>

namespace nx {
namespace plugins {
namespace flir {

class OnvifResourceSearcher: public ::OnvifResourceSearcher
{

public:
    OnvifResourceSearcher(QnCommonModule* commonModule);

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    virtual QnResourceList findResources() override;

    virtual bool isSequential() const override;
};

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)
