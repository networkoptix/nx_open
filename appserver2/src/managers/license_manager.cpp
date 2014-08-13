#include "license_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{

    ////////////////////////////////////////////////////////////
    //// class QnLicenseNotificationManager
    ////////////////////////////////////////////////////////////
    void QnLicenseNotificationManager::triggerNotification( const QnTransaction<ApiLicenseDataList>& tran )
    {
        QnLicenseList licenseList;
        fromApiToResourceList(tran.params, licenseList);

        foreach (const QnLicensePtr& license, licenseList) {
            emit licenseChanged(license);
        }
    }

    void QnLicenseNotificationManager::triggerNotification( const QnTransaction<ApiLicenseData>& tran )
    {
        QnLicensePtr license(new QnLicense());
        fromApiToResource(tran.params, license);
        if (tran.command == ApiCommand::addLicense)
            emit licenseChanged(license);
        else if (tran.command == ApiCommand::removeLicense)
            emit licenseRemoved(license);
    }



    ////////////////////////////////////////////////////////////
    //// class QnLicenseManager
    ////////////////////////////////////////////////////////////
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
                fromApiToResourceList(licenses, outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiLicenseDataList, decltype(queryDoneHandler)>( ApiCommand::getLicenses, nullptr, queryDoneHandler );
        return reqID;
    }
    
    template<class T>
    int QnLicenseManager<T>::addLicenses( const QnLicenseList& licenses, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //for( QnLicensePtr lic: licenses )
        //    lic->setId(QUuid::createUuid());

        auto tran = prepareTransaction( ApiCommand::addLicenses, licenses );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class T>
    int QnLicenseManager<T>::removeLicense( const QnLicensePtr& license, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //for( QnLicensePtr lic: licenses )
        //    lic->setId(QUuid::createUuid());

        auto tran = prepareTransaction( ApiCommand::removeLicense, license );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class T>
    QnTransaction<ApiLicenseDataList> QnLicenseManager<T>::prepareTransaction( ApiCommand::Value cmd, const QnLicenseList& licenses )
    {
        QnTransaction<ApiLicenseDataList> tran( cmd);
        fromResourceListToApi(licenses, tran.params);
        return tran;
    }

    template<class T>
    QnTransaction<ApiLicenseData> QnLicenseManager<T>::prepareTransaction( ApiCommand::Value cmd, const QnLicensePtr& license )
    {
        QnTransaction<ApiLicenseData> tran( cmd);
        fromResourceToApi(license, tran.params);
        return tran;
    }

    template class QnLicenseManager<ServerQueryProcessor>;
    template class QnLicenseManager<FixedUrlClientQueryProcessor>;
}
