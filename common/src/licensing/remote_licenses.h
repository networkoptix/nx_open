#pragma once

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <licensing/license.h>

/** Get list of licenses on the remote server. */
QnLicenseList remoteLicenses(const QUrl &url, const QAuthenticator &auth, int *status = nullptr);

