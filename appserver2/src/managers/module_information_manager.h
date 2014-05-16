#ifndef MODULE_INFORMATION_MANAGER_H
#define MODULE_INFORMATION_MANAGER_H

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_module_data.h>
#include <transaction/transaction.h>

namespace ec2 {

    template<class QueryProcessorType>
    class QnModuleInformationManager : public AbstractModuleInformationManager {
    public:
        QnModuleInformationManager(QueryProcessorType * const queryProcessor);
        virtual ~QnModuleInformationManager();

        void triggerNotification(const QnTransaction<ApiModuleData> &transaction);
        void triggerNotification(const ApiModuleDataList &moduleDataList);

    protected:
        virtual int sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiModuleData> prepareTransaction(const QnModuleInformation &moduleInformation, bool isAlive) const;
    };

} // namespace ec2

#endif // MODULE_INFORMATION_MANAGER_H
