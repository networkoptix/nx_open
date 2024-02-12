// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <core/resource/resource.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

namespace nx::vms::client::core {

class DesktopCameraConnection;

class NX_VMS_CLIENT_CORE_API DesktopResource: public QnAbstractArchiveResource
{
    Q_OBJECT

public:
    DesktopResource();
    virtual ~DesktopResource();

    static nx::Uuid getDesktopResourceUuid();

    bool isConnected() const;
    void initializeConnection(const QnMediaServerResourcePtr& server, const nx::Uuid& userId);
    void disconnectFromServer();

    virtual bool isRendererSlow() const = 0;
    virtual AudioLayoutConstPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider) const override;

    static QString calculateUniqueId(const nx::Uuid& moduleId, const nx::Uuid& userId);

private:
    std::unique_ptr<DesktopCameraConnection> m_connection;
};

} // namespace nx::vms::client::core
