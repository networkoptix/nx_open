#ifndef __RUNTIME_INFO_MANAGER_H_
#define __RUNTIME_INFO_MANAGER_H_

#include <QMap>
#include "nx_ec/data/api_fwd.h"
#include "nx_ec/data/api_runtime_data.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "utils/common/id.h"

class QnRuntimeInfoManager: public QObject
{
    Q_OBJECT
public:
    QnRuntimeInfoManager();
    virtual ~QnRuntimeInfoManager();

    static QnRuntimeInfoManager* instance();

    void update(const ec2::ApiRuntimeData& runtimeInfo);
    QMap<QnId, ec2::ApiRuntimeData> allData() const;
    ec2::ApiRuntimeData data(const QnId& id) const;

signals:
    void runtimeInfoChanged(ec2::ApiRuntimeData data);
private slots:
    void at_runtimeInfoChanged(const ec2::ApiRuntimeData &runtimeInfo);
    void at_remotePeerFound(const ec2::ApiPeerAliveData &data, bool isProxy);
    void at_remotePeerLost(const ec2::ApiPeerAliveData &data, bool isProxy);

private:
    QMap<QnId, ec2::ApiRuntimeData> m_runtimeInfo;
    mutable QMutex m_mutex;
};

#endif
