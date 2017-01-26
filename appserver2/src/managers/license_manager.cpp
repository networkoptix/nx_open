#include "license_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{

    ////////////////////////////////////////////////////////////
    //// class QnLicenseNotificationManager
    ////////////////////////////////////////////////////////////
    void QnLicenseNotificationManager::triggerNotification( const QnTransaction<ApiLicenseDataList>& tran, NotificationSource /*source*/)
    {
        QnLicenseList licenseList;
        fromApiToResourceList(tran.params, licenseList);

        for (const QnLicensePtr& license: licenseList) {
            emit licenseChanged(license);
        }
    }

    void QnLicenseNotificationManager::triggerNotification( const QnTransaction<ApiLicenseData>& tran, NotificationSource /*source*/)
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
    QnLicenseManager<T>::QnLicenseManager(T* const queryProcessor, const Qn::UserAccessData &userAccessData)
    :
      m_queryProcessor( queryProcessor ),
      m_userAccessData(userAccessData)
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
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<std::nullptr_t, ApiLicenseDataList, decltype(queryDoneHandler)>( ApiCommand::getLicenses, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnLicenseManager<T>::addLicenses( const QnLicenseList& licenses, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiLicenseDataList params;
        fromResourceListToApi(licenses, params);

        using namespace std::placeholders;
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::addLicenses, params,
            std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class T>
    int QnLicenseManager<T>::removeLicense( const QnLicensePtr& license, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiLicenseData params;
        fromResourceToApi(license, params);

        using namespace std::placeholders;
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeLicense, params,
            std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template class QnLicenseManager<ServerQueryProcessorAccess>;
    template class QnLicenseManager<FixedUrlClientQueryProcessor>;
}
