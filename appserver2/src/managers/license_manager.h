
#ifndef EC2_LICENSE_MANAGER_H
#define EC2_LICENSE_MANAGER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_license_data.h>

#include "transaction/transaction.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnLicenseManager
    :
        public AbstractLicenseManager
    {
    public:
        QnLicenseManager( QueryProcessorType* const queryProcessor );

        virtual int getLicenses( impl::GetLicensesHandlerPtr handler ) override;
        virtual int addLicenses( const QnLicenseList& licenses, impl::SimpleHandlerPtr handler ) override;

        void triggerNotification( const QnTransaction<ApiLicenseDataList>& tran );
        void triggerNotification( const QnTransaction<ApiLicenseData>& tran );

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiLicenseDataList> prepareTransaction( ApiCommand::Value cmd, const QnLicenseList& licenses );
    };
}

#endif  //EC2_LICENSE_MANAGER_H
