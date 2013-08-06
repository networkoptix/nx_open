#ifndef APPLAUNCHER_UTILS_H
#define APPLAUNCHER_UTILS_H

#include <connectinfo.h>
#include <utils/common/software_version.h>

/**
 * Send the required version to the applauncher,
 * close the current instance of the client.
 */
bool restartClient(const QnSoftwareVersion &version = QnSoftwareVersion(),
                   const QByteArray &auth = QByteArray());


#endif // APPLAUNCHER_UTILS_H
