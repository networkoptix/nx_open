#include "discovery_manager.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#endif
#include <cstdio>

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/network/http/http_client.h>
#include <nx/network/http/multipart_content_parser.h>

#include "camera_manager.h"
#include "plugin.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/log/log_main.h>

#define UrlPathReplaceRecord_Fields (fromPath)(toPath)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((UrlPathReplaceRecord), (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((UrlPathReplaceRecord), (json), _Fields)

namespace nx::vms_server_plugins::mjpeg_link {

DiscoveryManager::DiscoveryManager(nxpt::CommonRefManager* const refManager,
                                   nxpl::TimeProvider *const timeProvider)
:
    m_refManager( refManager ),
    m_timeProvider( timeProvider )
{
    QByteArray data;
    QFile file(":/mjpeg_link_plugin/manifest.json");
    if (file.open(QFile::ReadOnly))
        m_replaceData = QJson::deserialized<std::vector<UrlPathReplaceRecord>>(file.readAll());
    {
        QFile file("plugins/mjpeg_link_plugin/manifest.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this,
                lm("Switch to external manifest file %1").arg(QFileInfo(file).absoluteFilePath()));
            m_replaceData = QJson::deserialized<std::vector<UrlPathReplaceRecord>>(file.readAll());
        }
    }
}

void* DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        addRef();
        return this;
    }
    if (memcmp(&interfaceID, &nxcip::IID_CameraDiscoveryManager2, sizeof(nxcip::IID_CameraDiscoveryManager2)) == 0)
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int DiscoveryManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int DiscoveryManager::releaseRef()
{
    return m_refManager.releaseRef();
}

static const char* VENDOR_NAME = "HTTP_URL_PLUGIN";

void DiscoveryManager::getVendorName( char* buf ) const
{
    strcpy( buf, VENDOR_NAME );
}

int DiscoveryManager::findCameras( nxcip::CameraInfo* /*cameras*/, const char* /*localInterfaceIPAddr*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int DiscoveryManager::findCameras2(nxcip::CameraInfo2* /*cameras*/, const char* /*localInterfaceIPAddr*/)
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

static const QString HTTP_PROTO_NAME( QString::fromLatin1("http") );
static const QString HTTPS_PROTO_NAME( QString::fromLatin1("https") );

bool DiscoveryManager::validateUrl(const nx::utils::Url& url)
{
    nxpt::ScopedRef<HttpLinkPlugin> plugin(HttpLinkPlugin::instance());
    if (!plugin)
        return false;

    if (plugin->isStreamRunning(url))
        return true;

    nx::network::http::HttpClient httpClient;
    if (!httpClient.doGet(url))
    {
        NX_DEBUG(typeid(DiscoveryManager), "Request %1 has failed", url);
        return false;
    }

    //checking content-type
    nx::network::http::MultipartContentParser multipartContentParser;
    if (nx::network::http::strcasecmp(httpClient.contentType(), "image/jpeg") != 0 && //not a motion jpeg
        !multipartContentParser.setContentType(httpClient.contentType()))  //not a single jpeg
    {
        NX_DEBUG(typeid(DiscoveryManager), "Response %1 has wrong content type", url);
        return false;
    }

    NX_DEBUG(typeid(DiscoveryManager), "Url %1 is successfuly verified", url);
    return true;
}

QList<nx::utils::Url> DiscoveryManager::translateUrlHook(const nx::utils::Url& url) const
{
    QList<nx::utils::Url> result;
    const QString path(url.path());
    for (const auto& value: m_replaceData)
    {
        if (path == value.fromPath)
        {
            nx::utils::Url urlToCheck(url);
            urlToCheck.setPath(value.toPath);
            if (validateUrl(urlToCheck))
                result << urlToCheck;
        }
    }
    if (result.isEmpty())
    {
        if (validateUrl(url))
            result << url;
    }
    return result;
}

QString DiscoveryManager::getGroupName(const nx::utils::Url& url) const
{
    const QString path(url.path());
    for (const auto& value : m_replaceData)
    {
        if (value.toPath == path)
        {
            int counter = 0;
            for (const auto& value2: m_replaceData)
            {
                if (value2.fromPath == value.fromPath)
                    ++counter;
            }
            if (counter > 2)
            {
                nx::utils::Url u(url);
                u.setPath(value.fromPath);
                return u.toString(QUrl::RemoveUserInfo);
            }
        }
    }
    return QString(); //< Not a multi channel camera.
}

int DiscoveryManager::checkHostAddress(nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password)
{
    return 0;
}

int DiscoveryManager::checkHostAddress2(
    nxcip::CameraInfo2* cameras, const char* address, const char* login, const char* password)
{
    nx::utils::Url url( QString::fromUtf8(address) );
    if (url.scheme() != HTTP_PROTO_NAME && url.scheme() != HTTPS_PROTO_NAME)
        return 0;

    if (login)
        url.setUserName(QLatin1String(login));
    if (password)
        url.setPassword(QLatin1String(password));

    auto urlList = translateUrlHook(url);
    int cameraCount = 0;
    for (int i = 0; i < urlList.size(); ++i)
    {
        nx::utils::Url url(urlList[i]);
        url.setUserName(QString());
        url.setPassword(QString());
        QByteArray urlStr = url.toString().toUtf8();

        const QByteArray& uidStr = QCryptographicHash::hash(urlStr, QCryptographicHash::Md5).toHex();

        auto& camera = cameras[cameraCount];
        memset(&camera, 0, sizeof(camera));
        strncpy(camera.uid, uidStr.constData(), sizeof(camera.uid) - 1);
        strncpy(camera.url, urlStr, sizeof(camera.url) - 1);
        strncpy(camera.modelName, urlStr.data(), sizeof(camera.modelName) - 1);
        if (login)
            strncpy(camera.defaultLogin, login, sizeof(camera.defaultLogin) - 1);
        if (password)
            strncpy(camera.defaultPassword, password, sizeof(camera.defaultPassword) - 1);

        QString groupName = getGroupName(url);
        if (!groupName.isEmpty())
        {
            const QByteArray& groupId = QCryptographicHash::hash(groupName.toUtf8(), QCryptographicHash::Md5).toHex();
            strncpy(camera.groupName, groupName.toUtf8().data(), sizeof(camera.groupName) - 1);
            strncpy(camera.groupId, groupId.data(), sizeof(camera.groupId) - 1);
        }

        ++cameraCount;
    }
    return cameraCount;
}

int DiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NO_ERROR;
}

nxcip::BaseCameraManager* DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
{
    return new CameraManager(info, m_timeProvider);
}

int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
{
    *count = 0;
    return nxcip::NX_NO_ERROR;
}

} // namespace nx::vms_server_plugins::mjpeg_link
