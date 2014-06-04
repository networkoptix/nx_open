#ifndef MODULE_INFORMATION_MANAGER_H
#define MODULE_INFORMATION_MANAGER_H

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_module_data.h>
#include <transaction/transaction.h>

namespace ec2 {

    template<class QueryProcessorType>
    class QnMiscManager : public AbstractMiscManager {
    public:
        QnMiscManager(QueryProcessorType * const queryProcessor);
        virtual ~QnMiscManager();

        void triggerNotification(const QnTransaction<ApiModuleData> &transaction);
        void triggerNotification(const ApiModuleDataList &moduleDataList);
        void triggerNotification(const QnTransaction<QString> &transaction);

    protected:
        virtual int sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, impl::SimpleHandlerPtr handler) override;
        virtual int changeSystemName(const QString &systemName, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiModuleData> prepareTransaction(const QnModuleInformation &moduleInformation, bool isAlive) const;
        QnTransaction<QString> prepareTransaction(const QString &systemName) const;
    };

} // namespace ec2

#endif // MODULE_INFORMATION_MANAGER_H
