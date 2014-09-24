
#ifndef EC2_LICENSE_MANAGER_H
#define EC2_LICENSE_MANAGER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_license_data.h>

#include "transaction/transaction.h"


namespace ec2
{
    class QnLicenseNotificationManager
    :
        public AbstractLicenseManager
    {
    public:
        void triggerNotification( const QnTransaction<ApiLicenseDataList>& tran );
        void triggerNotification( const QnTransaction<ApiLicenseData>& tran );
    };



    template<class QueryProcessorType>
    class QnLicenseManager
    :
        public QnLicenseNotificationManager
    {
    public:
        QnLicenseManager( QueryProcessorType* const queryProcessor );

        virtual int getLicenses( impl::GetLicensesHandlerPtr handler ) override;
        virtual int addLicenses( const QnLicenseList& licenses, impl::SimpleHandlerPtr handler ) override;
        virtual int removeLicense( const QnLicensePtr& license, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiLicenseDataList> prepareTransaction( ApiCommand::Value cmd, const QnLicenseList& licenses );
        QnTransaction<ApiLicenseData> prepareTransaction( ApiCommand::Value cmd, const QnLicensePtr& license );
    };
}

#endif  //EC2_LICENSE_MANAGER_H
