#include "flir_onvif_resource_searcher.h"

#if defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)

namespace {

const int kFlirDefaultOnvifPort = 8090;

} // namespace

namespace nx {
namespace plugins {
namespace flir {

OnvifResourceSearcher::OnvifResourceSearcher(QnMediaServerModule* serverModule)
    :
    QnAbstractResourceSearcher(serverModule->commonModule()),
    ::OnvifResourceSearcher(serverModule)
{
}

bool OnvifResourceSearcher::isSequential() const
{
    return true;
}

QnResourceList OnvifResourceSearcher::findResources()
{
    return QnResourceList();
}

QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddr(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    bool doMultichannelCheck)
{
    auto port = url.port(kFlirDefaultOnvifPort);
    auto urlCopy = url;
    urlCopy.setPort(port);

    return ::OnvifResourceSearcher::checkHostAddr(urlCopy, auth, doMultichannelCheck);
}

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)
