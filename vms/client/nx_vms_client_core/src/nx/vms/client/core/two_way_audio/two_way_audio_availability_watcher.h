// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

namespace nx::vms::license { class SingleCamLicenseStatusHelper; }

namespace nx::vms::client::core {

/**
 * Watches on the specified camera for the two way audio transmission availability. Takes into
 * the consideration mapping of the source camera to it's audio output device.
 */
class NX_VMS_CLIENT_CORE_API TwoWayAudioAvailabilityWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    TwoWayAudioAvailabilityWatcher(QObject* parent = nullptr);
    virtual ~TwoWayAudioAvailabilityWatcher() override;

    bool available() const;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    /* Device to be used as an actual audio output. */
    QnVirtualCameraResourcePtr audioOutputDevice();

signals:
    void availabilityChanged();
    void cameraChanged();
    void audioOutputDeviceChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
