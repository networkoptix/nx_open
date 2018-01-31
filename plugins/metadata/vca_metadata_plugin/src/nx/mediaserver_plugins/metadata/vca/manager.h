#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>

#include "nx/api/vca_analytics_event.h"
#include "nx/api/vca_driver_manifest.h"
#include "monitor.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

class Manager:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractMetadataManager>
{
    Q_OBJECT;
public:
    Manager(const nx::sdk::ResourceInfo& resourceInfo, const QByteArray& pluginManifest);

    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::AbstractMetadataHandler* handler,
        nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    const nx::api::VcaAnalyticsEventType& getVcaAnalyticsEventTypeByEventTypeId(
        const QnUuid& id) const;
    const nx::api::VcaAnalyticsEventType& getVcaAnalyticsEventTypeByInternalName(
        const char* name) const;

private:
    QUrl m_url;
    QAuthenticator m_auth;

    Monitor* m_monitor = nullptr;

    QByteArray m_cameraManifest;
    nx::api::VcaAnalyticsDriverManifest m_typedPluginManifest;
    nx::api::VcaAnalyticsEventType m_emptyEvent;
};

} // vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
