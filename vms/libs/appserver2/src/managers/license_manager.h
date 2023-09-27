// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/data/license_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_license_manager.h>
#include <transaction/transaction.h>

namespace ec2
{

template<class QueryProcessorType>
class QnLicenseManager: public AbstractLicenseManager
{
public:
    QnLicenseManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int getLicenses(
        Handler<QnLicenseList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int addLicenses(
        const QnLicenseList& licenses,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeLicense(
        const QnLicensePtr& license,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

//-------------------------------------------------------------------------------------------------
// Implementation.

template<class T>
QnLicenseManager<T>::QnLicenseManager(
    T* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class T>
int QnLicenseManager<T>::getLicenses(
    Handler<QnLicenseList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<std::nullptr_t, nx::vms::api::LicenseDataList>(
        ApiCommand::getLicenses,
        nullptr,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](
            Result result, const nx::vms::api::LicenseDataList& licenses) mutable
        {
            QnLicenseList outData;
            if (result)
                fromApiToResourceList(licenses, outData);
            handler(requestId, std::move(result), std::move(outData));
        }
    );
    return requestId;
}

template<class T>
int QnLicenseManager<T>::addLicenses(
    const QnLicenseList& licenses,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::LicenseDataList params;
    fromResourceListToApi(licenses, params);

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::addLicenses,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnLicenseManager<T>::removeLicense(
    const QnLicensePtr& license,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::LicenseData params;
    fromResourceToApi(license, params);

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeLicense,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
