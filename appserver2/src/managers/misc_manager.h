#ifndef MODULE_INFORMATION_MANAGER_H
#define MODULE_INFORMATION_MANAGER_H

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_module_data.h>
#include <nx_ec/data/api_system_name_data.h>
#include <transaction/transaction.h>

namespace ec2 {

    class QnMiscNotificationManager : public AbstractMiscManager {
    public:
        void triggerNotification(const QnTransaction<ApiModuleData> &transaction);
        void triggerNotification(const QnTransaction<ApiModuleDataList> &transaction);
        void triggerNotification(const QnTransaction<ApiSystemNameData> &transaction);
    };

    template<class QueryProcessorType>
    class QnMiscManager : public QnMiscNotificationManager {
    public:
        QnMiscManager(QueryProcessorType * const queryProcessor);
        virtual ~QnMiscManager();
        virtual int markLicenseOverflow(bool value, qint64 time, impl::SimpleHandlerPtr handler) override;
    protected:
        virtual int sendModuleInformation(const QnModuleInformationWithAddresses &moduleInformation, bool isAlive, impl::SimpleHandlerPtr handler) override;
        virtual int sendModuleInformationList(const QList<QnModuleInformationWithAddresses> &moduleInformationList, impl::SimpleHandlerPtr handler) override;
        virtual int changeSystemName(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime, impl::SimpleHandlerPtr handler) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiModuleData> prepareTransaction(const QnModuleInformationWithAddresses &moduleInformation, bool isAlive) const;
        QnTransaction<ApiModuleDataList> prepareTransaction(const QList<QnModuleInformationWithAddresses> &moduleInformationList) const;
        QnTransaction<ApiSystemNameData> prepareTransaction(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime) const;
    };

} // namespace ec2

#endif // MODULE_INFORMATION_MANAGER_H
