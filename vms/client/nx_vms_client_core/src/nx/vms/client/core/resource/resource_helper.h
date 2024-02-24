// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/types/resource_types.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::core {

class ResourceHelper: public QObject
{
    Q_OBJECT

    // Resource status should be int for correct interaction within QML code/enums.
    Q_PROPERTY(int resourceStatus READ resourceStatus NOTIFY resourceStatusChanged)

    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(bool hasDefaultCameraPassword READ hasDefaultCameraPassword
        NOTIFY defaultCameraPasswordChanged)
    Q_PROPERTY(bool hasOldCameraFirmware READ hasOldCameraFirmware
        NOTIFY oldCameraFirmwareChanged)

    Q_PROPERTY(bool audioSupported READ audioSupported NOTIFY audioSupportedChanged)
    Q_PROPERTY(bool isIoModule READ isIoModule NOTIFY isIoModuleChanged)
    Q_PROPERTY(bool isVideoCamera READ isVideoCamera NOTIFY isVideoCameraChanged)

    /** Whether it's a video or image resource. */
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)

    Q_PROPERTY(qint64 displayOffset READ displayOffset NOTIFY displayOffsetChanged)

public:
    ResourceHelper(QObject* parent = nullptr);

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr& value);

    int resourceStatus() const;
    QString resourceName() const;
    bool hasDefaultCameraPassword() const;
    bool hasOldCameraFirmware() const;

    bool audioSupported() const;
    bool isIoModule() const;
    bool hasVideo() const;

    /** Whether the resource is a camera with a video stream. */
    bool isVideoCamera() const;

    qint64 displayOffset() const;

signals:
    void resourceChanged();
    void resourceStatusChanged();
    void resourceNameChanged();
    void defaultCameraPasswordChanged();
    void oldCameraFirmwareChanged();
    void audioSupportedChanged();
    void isVideoCameraChanged();
    void isIoModuleChanged();
    void hasVideoChanged();
    void displayOffsetChanged();
    void resourceRemoved();

private:
    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

private:
    QnResourcePtr m_resource;
    nx::utils::ScopedConnections m_connections;
};

} // namespace nx::vms::client::core
