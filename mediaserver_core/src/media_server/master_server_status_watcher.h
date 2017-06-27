#pragma once

#include <QObject>
#include <QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <api/runtime_info_manager.h>
#include <common/common_module_aware.h>

/**
 * Monitor runtime flags RF_CloudSync. Only one server at once should has it
 */

class QnMasterServerStatusWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:
    QnMasterServerStatusWatcher(
        QObject* parent,
        std::chrono::milliseconds delayBeforeSettingMasterFlag);
    virtual ~QnMasterServerStatusWatcher() {}
private slots:
    void at_updateMasterFlag();
private:
    void setMasterFlag(bool value);
    bool localPeerCanBeMaster() const;
private:
    QTimer m_timer;
    std::chrono::milliseconds m_delayBeforeSettingMasterFlag;
};
