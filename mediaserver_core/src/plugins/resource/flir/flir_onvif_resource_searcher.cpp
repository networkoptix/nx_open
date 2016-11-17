#include "flir_onvif_resource_searcher.h" 

namespace {
    const int kFlirDefaultOnvifPort = 8090;
} // namespace

QList<QnResourcePtr> FlirOnvifResourceSearcher::checkHostAddr(
    const QUrl& url,
    const QAuthenticator& auth,
    bool doMultichannelCheck)
{
    auto port = url.port(kFlirDefaultOnvifPort);
    auto urlCopy = url;
    urlCopy.setPort(port);

    return OnvifResourceSearcher::checkHostAddr(urlCopy, auth, doMultichannelCheck);
}

QnResourceList FlirOnvifResourceSearcher::findResources()
{
    return QnResourceList();
}

bool FlirOnvifResourceSearcher::isSequential() const
{
    return true;
}

