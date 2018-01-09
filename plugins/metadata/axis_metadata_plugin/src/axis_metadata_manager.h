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
    AxisMetadataManager(AxisMetadataPlugin* plugin);

    virtual ~AxisMetadataManager();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error startFetchingMetadata(
        nx::sdk::metadata::AbstractMetadataHandler* handler,
        nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* capabilitiesManifest(nx::sdk::Error* error) const override;

    void setResourceInfo(const nx::sdk::ResourceInfo& resourceInfo);
    void setDeviceManifest(const QByteArray& manifest);
    void setDriverManifest(const Axis::DriverManifest& manifest);

    void setMonitor(AxisMetadataMonitor* monitor);

    nx::sdk::metadata::AbstractMetadataHandler* metadataHandler();
    QString sharedId();
    AxisMetadataPlugin* plugin();
    QList<AxisEvent> m_axisEvents;

private:
    Axis::DriverManifest m_driverManifest;
    QByteArray m_deviceManifest;

    QUrl m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channel;

    AxisMetadataPlugin* m_plugin = nullptr;
    AxisMetadataMonitor* m_monitor = nullptr;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler = nullptr;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx

//#endif // defined(ENABLE_HANWHA)
