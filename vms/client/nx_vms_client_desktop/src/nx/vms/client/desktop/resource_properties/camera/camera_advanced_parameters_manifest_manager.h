#pragma once

#include <optional>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_advanced_param.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/**
 * Camera advanced parameters are descibed in the manifest structure. It is persistent and does not
 * depend on server, so can be loaded once per connection session. Manager loads these manifests on
 * request and notifies when loading is complete.
 */
class CameraAdvancedParametersManifestManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraAdvancedParametersManifestManager(QObject* parent = nullptr);
    virtual ~CameraAdvancedParametersManifestManager() override;

    /** Get manifest if it is loaded. */
    std::optional<QnCameraAdvancedParams> manifest(const QnVirtualCameraResourcePtr& camera) const;

    /**
     * Request to load manifest. When manifest is successfully loaded, corresponding signal will be
     * emitted. In case of failure nothing occurs.
     */
    void loadManifestAsync(const QnVirtualCameraResourcePtr& camera);

signals:
    void manifestLoaded(
        const QnVirtualCameraResourcePtr& camera,
        const QnCameraAdvancedParams& manifest);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
