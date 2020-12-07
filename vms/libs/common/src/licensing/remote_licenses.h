#pragma once

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/url.h>
#include <licensing/license.h>
#include <utils/merge_systems_common.h>

/** Get list of licenses on the remote server. */
QnLicenseList remoteLicenses(
    const nx::utils::Url& url, const QAuthenticator& auth, int* status = nullptr);

/**
 * Check for licenses conflict between local and remote license pools - some kind of licenses are
 * allowed to exist only once per system.
 */
utils::MergeSystemsStatus::Value remoteLicensesConflict(
    QnLicensePool* localLicensesPool,
    const nx::utils::Url& url,
    const QAuthenticator& auth);
