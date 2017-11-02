#include "flir_onvif_resource_searcher.h" 

#if defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)

namespace {

const int kFlirDefaultOnvifPort = 8090;

} // namespace

namespace nx {
namespace plugins {
namespace flir {


OnvifResourceSearcher::OnvifResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    ::OnvifResourceSearcher(commonModule)
{

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

QnResourceList OnvifResourceSearcher::findResources()
{
    return QnResourceList();
}

bool OnvifResourceSearcher::isSequential() const
{
    return true;
}

} // namespace flir
} // namespace plugins
} // namespace nx


#endif // defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)

