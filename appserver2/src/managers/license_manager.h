
#ifndef EC2_LICENSE_MANAGER_H
#define EC2_LICENSE_MANAGER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/ec2_license.h>

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

        void triggerNotification( const QnTransaction<ApiLicense>& tran );

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiLicenseList> prepareTransaction( ApiCommand::Value cmd, const QnLicenseList& licenses );
    };
}

#endif  //EC2_LICENSE_MANAGER_H
