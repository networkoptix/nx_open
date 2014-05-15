#ifndef MODULE_INFORMATION_H
#define MODULE_INFORMATION_H

#include <QtCore/QString>
#include <QtCore/QSet>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

struct QnModuleInformation {
    QString type;
    QnSoftwareVersion version;
    QnSystemInformation systemInformation;
    QString systemName;
    QMap<QString, QString> parameters;
    QSet<QString> remoteAddresses;
    bool isLocal; //!< true if at least one address from \a remoteHostAddress is a local address
    QString id;

    QnModuleInformation() : isLocal(false) {}
};

#endif // MODULE_INFORMATION_H
