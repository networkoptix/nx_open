#pragma once

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/url.h>
#include <licensing/license.h>

/** Get list of licenses on the remote server. */
QnLicenseList remoteLicenses(const nx::utils::Url &url, const QAuthenticator &auth, int *status = nullptr);

