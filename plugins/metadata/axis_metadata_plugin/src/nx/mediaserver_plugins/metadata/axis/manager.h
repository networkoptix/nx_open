#pragma once

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/camera_manager.h>

#include "common.h"
#include "monitor.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

class Manager: public nxpt::CommonRefCounter<nx::sdk::metadata::CameraManager>
{
public:
    Manager(const nx::sdk::CameraInfo& cameraInfo,
        const AnalyticsDriverManifest& typedManifest);

    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::MetadataHandler* handler,
        nxpl::NX_GUID* typeList,
        int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    const QList<AnalyticsEventTypeExtended>& events() const noexcept
    {
        return m_events;
    }

    const AnalyticsEventTypeExtended& eventByUuid(const QnUuid& uuid) const noexcept;

private:
    QList<AnalyticsEventTypeExtended> m_events;
    QByteArray m_manifest;
    QUrl m_url;
    QAuthenticator m_auth;

    Monitor* m_monitor = nullptr;
    QList<QByteArray> m_givenManifests; //< Place to store manifests we gave to caller
    // to provide 1) sufficient lifetime and 2) memory releasing in destructor.
};

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
