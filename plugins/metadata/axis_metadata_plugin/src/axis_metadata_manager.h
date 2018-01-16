#pragma once

//#if defined(ENABLE_HANWHA)
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>

#include "axis_metadata_monitor.h"
#include "axis_metadata_plugin.h"

namespace nx {
namespace mediaserver {
namespace plugins {

class AxisMetadataManager:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataManager>
{
    Q_OBJECT;
public:
    AxisMetadataManager(const nx::sdk::ResourceInfo& resourceInfo,
        const QList<SupportedEventEx>& axisEvents);

    virtual ~AxisMetadataManager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::AbstractMetadataHandler* handler,
        nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;

    const QList<SupportedEventEx>& axisEvents() const { return m_axisEvents; }

private:
    QByteArray m_deviceManifest;
    QUrl m_url;
    QAuthenticator m_auth;
    QList<SupportedEventEx> m_axisEvents;
    AxisMetadataMonitor* m_monitor = nullptr;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx

//#endif // defined(ENABLE_HANWHA)
