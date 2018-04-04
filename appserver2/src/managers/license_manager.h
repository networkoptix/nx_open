#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_license_data.h>

#include "transaction/transaction.h"
#include "nx_ec/data/api_conversion_functions.h"

namespace ec2
{

    template<class QueryProcessorType>
    class QnLicenseManager : public AbstractLicenseManager
    {
    public:
        QnLicenseManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

        virtual int getLicenses( impl::GetLicensesHandlerPtr handler ) override;
        virtual int addLicenses( const QnLicenseList& licenses, impl::SimpleHandlerPtr handler ) override;
        virtual int removeLicense( const QnLicensePtr& license, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };

    
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

} // namespace ec2
