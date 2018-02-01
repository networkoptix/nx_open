#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>

#include "identified_supported_event.h"
#include "monitor.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

class Manager: public nxpt::CommonRefCounter<nx::sdk::metadata::AbstractCameraManager>
{
public:
    Manager(const nx::sdk::CameraInfo& cameraInfo,
        const QList<IdentifiedSupportedEvent>& events);

    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::AbstractMetadataHandler* handler,
        nxpl::NX_GUID* typeList,
        int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    const QList<IdentifiedSupportedEvent>& identifiedSupportedEvents() const
    {
        return m_identifiedSupportedEvents;
    }

private:
    // QByteArray m_deviceManifestPartial; //< Guids only - for test purposes.
    QByteArray m_deviceManifestFull; //< Guids and description.
    QUrl m_url;
    QAuthenticator m_auth;

    QList<IdentifiedSupportedEvent> m_identifiedSupportedEvents;

    Monitor* m_monitor = nullptr;
    QList<QByteArray> m_givenManifests; //< Place to store manifests we gave to caller
    // to provide 1) sufficient lifetime and 2) memory releasing in destructor.
};

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
