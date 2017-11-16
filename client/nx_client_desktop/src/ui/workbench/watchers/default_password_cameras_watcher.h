#pragma once

#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <common/common_module_aware.h>

namespace nx {
namespace client {
namespace desktop {

class DefaultPasswordCamerasWatcher: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    DefaultPasswordCamerasWatcher(QObject* parent = nullptr);
    virtual ~DefaultPasswordCamerasWatcher() override;

    bool notificationIsVisible() const;

    QnVirtualCameraResourceList camerasWithDefaultPassword() const;

private:
    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

signals:
    void notificationIsVisibleChanged();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
