// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/url.h>
#include <nx/network/socket_factory.h>
#include <licensing/license.h>
#include <utils/merge_systems_common.h>

namespace nx::vms::license {

/** Get list of licenses on the remote server. */
QnLicenseList remoteLicenses(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    nx::network::ssl::AdapterFunc adapterFunc,
    int* status = nullptr);

/** Check if local pool has unique licenses. */
bool hasUniqueLicenses(QnLicensePool* localLicensesPool);

/**
 * Check for licenses conflict between local and remote license pools - some kind of licenses are
 * allowed to exist only once per system.
 */
MergeSystemsStatus remoteLicensesConflict(
    QnLicensePool* localLicensesPool,
    const QnLicenseList& remoteLicenseList);

/**
 * Check for licenses conflict between local and remote license pools - some kind of licenses are
 * allowed to exist only once per system.
 * Request remote licenses if need.
 */
MergeSystemsStatus remoteLicensesConflict(
    QnLicensePool* localLicensesPool,
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    nx::network::ssl::AdapterFunc adapterFunc);

} // namespace nx::vms::license
