// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>

class QnUuid;
namespace nx::vms::license { class SingleCamLicenseStatusHelper; }

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API TwoWayAudioAvailabilityWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    TwoWayAudioAvailabilityWatcher(QObject* parent = nullptr);
    virtual ~TwoWayAudioAvailabilityWatcher() override;

    void setCamera(const QnVirtualCameraResourcePtr& camera);

    bool available() const;

signals:
    void availabilityChanged();

private:
    void updateAvailability();

    void setAvailable(bool value);

private:
    bool m_available = false;
    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<nx::vms::license::SingleCamLicenseStatusHelper> m_licenseHelper;
    nx::utils::ScopedConnections m_connections;
};

} // namespace nx::vms::client::core
