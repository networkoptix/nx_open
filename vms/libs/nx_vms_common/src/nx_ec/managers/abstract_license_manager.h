// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/utils/async_handler_executor.h>
#include <licensing/license_fwd.h>

#include <QtCore/QObject>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractLicenseNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void licenseChanged(QSharedPointer<QnLicense> license);
    void licenseRemoved(QSharedPointer<QnLicense> license);
};

/*!
    \note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractLicenseManager
{
public:
    virtual ~AbstractLicenseManager() = default;

    virtual int getLicenses(
        Handler<QnLicenseList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getLicensesSync(QnLicenseList* outLicenses);

    virtual int addLicenses(
        const QnLicenseList& licenses,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode addLicensesSync(const QnLicenseList& licenses);

    virtual int removeLicense(
        const QnLicensePtr& license,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode removeLicenseSync(const QnLicensePtr& license);
};

} // namespace ec2
