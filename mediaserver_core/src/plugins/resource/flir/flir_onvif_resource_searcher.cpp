#include "flir_onvif_resource_searcher.h" 

#if defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)

namespace {

const int kFlirDefaultOnvifPort = 8090;

} // namespace

using namespace nx::plugins;

QList<QnResourcePtr> flir::OnvifResourceSearcher::checkHostAddr(
    const QUrl& url,
    const QAuthenticator& auth,
    bool doMultichannelCheck)
{
    auto port = url.port(kFlirDefaultOnvifPort);
    auto urlCopy = url;
    urlCopy.setPort(port);

    return ::OnvifResourceSearcher::checkHostAddr(urlCopy, auth, doMultichannelCheck);
}

QnResourceList flir::OnvifResourceSearcher::findResources()
{
    return QnResourceList();
}

bool flir::OnvifResourceSearcher::isSequential() const
{
    return true;
}

#endif // defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)

