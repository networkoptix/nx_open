// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

class QnUuid;
namespace nx::vms::license { class SingleCamLicenseStatusHelper; }

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API TwoWayAudioAvailabilityWatcher:
    public QObject,
    public CommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    TwoWayAudioAvailabilityWatcher(QObject* parent = nullptr);

    virtual ~TwoWayAudioAvailabilityWatcher() override;

    void setResourceId(const QnUuid& uuid);

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
};

} // namespace nx::vms::client::core
