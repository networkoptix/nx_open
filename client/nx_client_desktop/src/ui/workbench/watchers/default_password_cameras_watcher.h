#pragma once

#include <unordered_set>

#include <QtCore/QUuid>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <common/common_module_aware.h>

namespace nx {
namespace client {
namespace desktop {

class DefaultPasswordCamerasWatcher: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QnCommonModuleAware;

public:
    DefaultPasswordCamerasWatcher(QObject* parent = nullptr);

    bool notificationIsVisible() const;

    QnVirtualCameraResourceList camerasWithDefaultPassword() const;

signals:
    void notificationIsVisibleChanged();
    void camerasWithDefaultPasswordChanged();

private:
    bool m_notificationIsVisible = false;
    QHash<QUuid, QnVirtualCameraResourcePtr> m_camerasWithDefaultPassword;
};

} // namespace desktop
} // namespace client
} // namespace nx
