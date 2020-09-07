#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;
class CameraAdvancedParametersManifestManager;

/**
 * Asynchronously loads advanced settings manifest into the store and refreshes it when camera is
 * re-initialized. As we cannot know it for sure, camera status is used as an indirect sign of it.
 */
class CameraSettingsAdvancedManifestWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsAdvancedManifestWatcher(
        CameraAdvancedParametersManifestManager* manager,
        CameraSettingsDialogStore* store,
        QObject* parent = nullptr);
    virtual ~CameraSettingsAdvancedManifestWatcher() override;

    void selectCamera(const QnVirtualCameraResourcePtr& camera);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
