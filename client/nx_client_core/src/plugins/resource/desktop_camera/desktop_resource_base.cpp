#include "desktop_resource_base.h"
#include <core/resource/media_server_resource.h>

#include <plugins/resource/desktop_camera/desktop_camera_connection.h>
#include <plugins/resource/desktop_camera/desktop_data_provider_base.h>

#include <utils/common/long_runable_cleanup.h>

namespace {
    const QnUuid kDesktopResourceUuid(lit("{B3B2235F-D279-4d28-9012-00DE1002A61D}"));
}

QnDesktopResource::QnDesktopResource()
{
    const auto kName = lit("Desktop");

    addFlags(Qn::local_live_cam | Qn::desktop_camera);
    setName(kName);
    setUrl(kName);
    setId(kDesktopResourceUuid);
}

QnDesktopResource::~QnDesktopResource()
{
    m_connectionPool.clear();
}

QnUuid QnDesktopResource::getDesktopResourceUuid()
{
    return kDesktopResourceUuid;
}

void QnDesktopResource::addConnection(const QnMediaServerResourcePtr &server, const QnUuid& userId)
{
    if (m_connectionPool.find(server->getId()) != m_connectionPool.cend())
        return;

    QnDesktopCameraConnectionPtr connection(new QnDesktopCameraConnection(this, server, userId));
    connection->start();
    m_connectionPool.emplace(server->getId(), std::move(connection));
}

void QnDesktopResource::removeConnection(const QnMediaServerResourcePtr &server)
{
    auto connection = m_connectionPool.find(server->getId());
    if (connection == m_connectionPool.end())
        return;

    if (auto cleanup = QnLongRunableCleanup::instance())
        cleanup->cleanupAsync(std::move(connection->second));
    m_connectionPool.erase(connection);
}

QnConstResourceAudioLayoutPtr QnDesktopResource::getAudioLayout(const QnAbstractStreamDataProvider *dataProvider) const
{
    auto desktopProvider = dynamic_cast<QnDesktopDataProviderBase*>(
        const_cast<QnAbstractStreamDataProvider*>(dataProvider));
    if (desktopProvider)
        return desktopProvider->getAudioLayout();

    return QnConstResourceAudioLayoutPtr(nullptr);
}

QString QnDesktopResource::calculateUniqueId(const QnUuid& moduleId, const QnUuid& userId)
{
    return moduleId.toString() + userId.toString();
}
