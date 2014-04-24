
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

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiLicenseDataList& licenses ) {
            QnLicenseList outData;
            if( errorCode == ErrorCode::ok )
                licenses.toResourceList(outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<nullptr_t, ApiLicenseDataList, decltype(queryDoneHandler)>( ApiCommand::getLicenses, nullptr, queryDoneHandler );
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
    QnTransaction<ApiLicenseDataList> QnLicenseManager<T>::prepareTransaction( ApiCommand::Value cmd, const QnLicenseList& licenses )
    {
        QnTransaction<ApiLicenseDataList> tran( cmd, true );
        tran.params.fromResourceList( licenses );
        return tran;
    }

    template<class T>
    void QnLicenseManager<T>::triggerNotification( const QnTransaction<ApiLicenseDataList>& tran )
    {
        QnLicenseList licenseList;
        tran.params.toResourceList(licenseList);

        foreach (const QnLicensePtr& license, licenseList) {
            emit licenseChanged(license);
        }
    }

    template<class T>
    void QnLicenseManager<T>::triggerNotification( const QnTransaction<ApiLicenseData>& tran )
    {
        QnLicensePtr license(new QnLicense());
        tran.params.toResource(*license.data());
        emit licenseChanged(license);
    }

    template class QnLicenseManager<ServerQueryProcessor>;
    template class QnLicenseManager<FixedUrlClientQueryProcessor>;
}
