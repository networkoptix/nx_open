#pragma once

#include <QObject>
#include <QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <api/runtime_info_manager.h>

/**
 * Monitor runtime flags RF_CloudSync. Only one server at once should has it
 */

class QnMasterServerStatusWatcher: public QObject
{
    Q_OBJECT
public:
    QnMasterServerStatusWatcher();
    virtual ~QnMasterServerStatusWatcher() {}
private slots:
    void at_updateMasterFlag();
private:
    void setMasterFlag(bool value);
    bool localPeerCanBeMaster() const;
private:
    QTimer m_timer;
};
