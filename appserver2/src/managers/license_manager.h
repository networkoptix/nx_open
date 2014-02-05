
#ifndef EC2_LICENSE_MANAGER_H
#define EC2_LICENSE_MANAGER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>


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
        virtual int addLicensesAsync( const QList<QnLicensePtr>& licenses, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
    };
}

#endif  //EC2_LICENSE_MANAGER_H
