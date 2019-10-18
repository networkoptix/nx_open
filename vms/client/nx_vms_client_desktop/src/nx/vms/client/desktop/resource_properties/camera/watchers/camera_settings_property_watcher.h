#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsPropertyWatcher: public QObject
{
    using base_type = QObject;

public:
    CameraSettingsPropertyWatcher(
        CameraSettingsDialogStore* store, QObject* parent = nullptr);

    virtual ~CameraSettingsPropertyWatcher() override;

    QnVirtualCameraResourceSet cameras() const;
    void setCameras(const QnVirtualCameraResourceSet& value);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
