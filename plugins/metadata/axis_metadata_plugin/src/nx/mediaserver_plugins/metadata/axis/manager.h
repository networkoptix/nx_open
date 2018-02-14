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

class MetadataHandler;

class Manager: public nxpt::CommonRefCounter<nx::sdk::metadata::CameraManager>
{
public:
    Manager(const nx::sdk::CameraInfo& cameraInfo,
        const AnalyticsDriverManifest& typedManifest);

    virtual ~Manager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::metadata::MetadataHandler* handler) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* typeList,
        int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    const AnalyticsDriverManifest& events() const noexcept
    {
        return m_typedManifest;
    }

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

    /** @return nullptr if not found */
    const AnalyticsEventType* eventByUuid(const QnUuid& uuid) const noexcept;

private:
    AnalyticsDriverManifest m_typedManifest;
    QByteArray m_manifest;
    QUrl m_url;
    QAuthenticator m_auth;
    Monitor* m_monitor = nullptr;
    QList<QByteArray> m_givenManifests; //< Place to store manifests we gave to caller
    // to provide 1) sufficient lifetime and 2) memory releasing in destructor.
    nx::sdk::metadata::MetadataHandler* m_handler = nullptr;
};

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
