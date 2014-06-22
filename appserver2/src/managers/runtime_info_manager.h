#ifndef __RUNTIME_INFO_MANAGER_H_
#define __RUNTIME_INFO_MANAGER_H_

#include <QMap>
#include "nx_ec/data/api_fwd.h"
#include "utils/common/id.h"

namespace ec2
{

class QnRuntimeInfoManager: public QObject
{
public:
    QnRuntimeInfoManager();
    virtual ~QnRuntimeInfoManager();

    static QnRuntimeInfoManager* instance();

    void update(const ec2::ApiRuntimeData& runtimeInfo);
    QMap<QnId, ec2::ApiRuntimeData> data();

public slots:
    void on_runtimeInfoChanged(const ec2::ApiRuntimeData& runtimeInfo);
signals:
    void runtimeInfoChanged(ec2::ApiRuntimeData data);
private slots:
    void at_runtimeInfoChanged(const ec2::ApiRuntimeData& runtimeInfo);
    void at_remotePeerFound(ec2::ApiPeerAliveData data, bool isProxy);
    void at_remotePeerLost(ec2::ApiPeerAliveData data, bool isProxy);
private:
    QMap<QnId, ApiRuntimeData> m_runtimeInfo;
};

}

#endif
