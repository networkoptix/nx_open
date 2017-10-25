#pragma once

#include <chrono>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <nx/utils/uuid.h>

#include <api/runtime_info_manager.h>
#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

class QnCommonModule;

namespace nx {
namespace vms {
namespace cloud_integration {

/**
 * Responsible for choosing a single server that is to synchronize data with cloud.
 * That done by setting RF_CloudSync server flag.
 * NOTE: It is possible that for some short period of time more than one server has this flag set.
 */
class QnMasterServerStatusWatcher:
    public QObject
{
    Q_OBJECT
public:
    QnMasterServerStatusWatcher(
        QnCommonModule* commonModule,
        std::chrono::milliseconds delayBeforeSettingMasterFlag);
    virtual ~QnMasterServerStatusWatcher() {}

private slots:
    void at_updateMasterFlag();

private:
    QnCommonModule* m_commonModule;
    QTimer m_timer;
    std::chrono::milliseconds m_delayBeforeSettingMasterFlag;

    void setMasterFlag(bool value);
    bool localPeerCanBeMaster() const;
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
