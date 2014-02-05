
#include "license_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class T>
    QnLicenseManager<T>::QnLicenseManager( T* const queryProcessor )
    :
        m_queryProcessor( queryProcessor )
    {
    }

    template<class T>
    int QnLicenseManager<T>::getLicenses( impl::GetLicensesHandlerPtr handler )
    {
        //TODO/IMPL
        return 0;
    }
    
    template<class T>
    int QnLicenseManager<T>::addLicensesAsync( const QList<QnLicensePtr>& licenses, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return 0;
    }



    template class QnLicenseManager<ServerQueryProcessor>;
    template class QnLicenseManager<FixedUrlClientQueryProcessor>;
}
