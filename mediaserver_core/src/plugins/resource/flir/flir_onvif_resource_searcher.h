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

    virtual QnResourceList checkEndpoint(
        const QUrl& url, const QAuthenticator& auth,
        const QString& physicalId, QnResouceSearchMode mode) override;

    virtual QnResourceList findResources() override;

    virtual bool isSequential() const override;
};

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)
