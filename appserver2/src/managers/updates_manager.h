#ifndef EC2_UPDATES_MANAGER_H
#define EC2_UPDATES_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/ec2_update_data.h"
#include "transaction/transaction.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"

namespace ec2 {

template<class QueryProcessorType>
class QnUpdatesManager : public AbstractUpdatesManager {
public:
    QnUpdatesManager(QueryProcessorType * const queryProcessor);
    virtual ~QnUpdatesManager();

    void triggerNotification(const QnTransaction<ApiUpdateData> &transaction);

protected:
    virtual int sendUpdatePackage(const QString &updateId, const QByteArray &data, const PeerList &peers, impl::SimpleHandlerPtr handler) override;
    virtual int installUpdate(const QString &updateId, const PeerList &peers, impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessorType* const m_queryProcessor;

    QnTransaction<ApiUpdateData> prepareTransaction(const QString &updateId, const QByteArray &data) const;
    QnTransaction<ApiUpdateData> prepareTransaction(const QString &updateId) const;
};

} // namespace ec2

#endif // EC2_UPDATES_MANAGER_H
