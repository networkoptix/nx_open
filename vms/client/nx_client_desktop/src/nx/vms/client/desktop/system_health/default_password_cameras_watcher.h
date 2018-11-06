#pragma once

#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <common/common_module_aware.h>

namespace nx::vms::client::desktop {

class DefaultPasswordCamerasWatcher:
    public Connective<QObject>,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit DefaultPasswordCamerasWatcher(QObject* parent = nullptr);
    virtual ~DefaultPasswordCamerasWatcher() override;

    QnVirtualCameraResourceSet camerasWithDefaultPassword() const;

signals:
    void cameraSetChanged();

private:
    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
