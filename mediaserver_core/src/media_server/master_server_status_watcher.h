#pragma once

#include <QObject>
#include <QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <api/runtime_info_manager.h>

/**
 * Monitor runtime flags RF_CloudSync. Only one server at once should has it
 */

class MasterServerStatusWatcher: public QObject
{
    Q_OBJECT
public:
    MasterServerStatusWatcher();
    virtual ~MasterServerStatusWatcher() {}
private slots:
    void at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo);
private:
    void setMasterFlag(bool value);
private:
    QTimer m_timer;
};
