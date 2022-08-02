// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <core/resource/resource.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

class QnDesktopCameraConnection;

class NX_VMS_CLIENT_CORE_API QnDesktopResource: public QnAbstractArchiveResource
{
    Q_OBJECT

public:
    QnDesktopResource();
    virtual ~QnDesktopResource();

    static QnUuid getDesktopResourceUuid();

    bool isConnected() const;
    void initializeConnection(const QnMediaServerResourcePtr& server, const QnUuid& userId);
    void disconnectFromServer();

    virtual bool isRendererSlow() const = 0;
    virtual AudioLayoutConstPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider) const override;

    static QString calculateUniqueId(const QnUuid& moduleId, const QnUuid& userId);

private:
    std::unique_ptr<QnDesktopCameraConnection> m_connection;
};

