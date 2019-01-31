#pragma once

#include <QtCore/QObject>
#include <nx/vms/api/data/software_version.h>
#include <common/common_module_aware.h>

class QTimerEvent;

namespace nx::vms::client::desktop {

/** This class keeps applaucher running. */
class ApplauncherGuard: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ApplauncherGuard(QnCommonModule* commonModule, QObject* parent = nullptr);

protected:
    virtual void timerEvent(QTimerEvent* event) override;
};

} // namespace nx::vms::client::desktop
