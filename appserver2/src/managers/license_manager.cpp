
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
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiLicenseList& licenses ) {
            QnLicenseList outData;
            if( errorCode == ErrorCode::ok )
                licenses.toResourceList(outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiLicenseList, decltype(queryDoneHandler)>( ApiCommand::getLicenses, nullptr, queryDoneHandler );
        return reqID;
    }
    
    template<class T>
    int QnLicenseManager<T>::addLicenses( const QnLicenseList& licenses, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //for( QnLicensePtr lic: licenses )
        //    lic->setId(QnId::createUuid());

        auto tran = prepareTransaction( ApiCommand::addLicenses, licenses );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class T>
    QnTransaction<ApiLicenseList> QnLicenseManager<T>::prepareTransaction( ApiCommand::Value cmd, const QnLicenseList& licenses )
    {
        QnTransaction<ApiLicenseList> tran( cmd, true );
        tran.params.fromResourceList( licenses );
        return tran;
    }



    template class QnLicenseManager<ServerQueryProcessor>;
    template class QnLicenseManager<FixedUrlClientQueryProcessor>;
}
