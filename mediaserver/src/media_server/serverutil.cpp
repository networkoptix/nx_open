#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>

#include <core/resource/media_server_resource.h>

#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include "version.h"

static QnMediaServerResourcePtr m_server;

void syncStoragesToSettings(const QnMediaServerResourcePtr &server)
{
    const QnAbstractStorageResourceList& storages = server->getStorages();

    MSSettings::roSettings()->beginWriteArray(QLatin1String("storages"));
    MSSettings::roSettings()->remove(QLatin1String(""));
    for (int i = 0; i < storages.size(); i++) {
        QnAbstractStorageResourcePtr storage = storages.at(i);
        MSSettings::roSettings()->setArrayIndex(i);
        MSSettings::roSettings()->setValue("path", storage->getUrl());
    }

    MSSettings::roSettings()->endArray();
}

QString authKey()
{
    return MSSettings::roSettings()->value("authKey").toString();
}

#ifdef EDGE_SERVER

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>

void mac_eth0(char  MAC_str[13], char** host)
{
    #define HWADDR_len 6
    int s,i;
    struct ifreq ifr;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    strcpy(ifr.ifr_name, "eth0");
    if (ioctl(s, SIOCGIFHWADDR, &ifr) != -1) {
        for (i=0; i<HWADDR_len; i++)
            sprintf(&MAC_str[i*2],"%02X",((unsigned char*)ifr.ifr_hwaddr.sa_data)[i]);
    }
    if((ioctl(s, SIOCGIFADDR, &ifr)) != -1) {
        const sockaddr_in* ip = (sockaddr_in*) &ifr.ifr_addr;
        *host = inet_ntoa(ip->sin_addr);
    }
    close(s);
}
#endif


QUuid serverGuid() {
    static QUuid guid;

    if (!guid.isNull())
        return guid;

    guid = MSSettings::roSettings()->value(lit("serverGuid")).toString();

#ifdef _TEST_TWO_SERVERS
    return guid + "test";
#endif
    return guid;
}

QString getDataDirectory()
{
    const QString& dataDirFromSettings = MSSettings::roSettings()->value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/mediaserver/var").arg(VER_LINUX_ORGANIZATION_NAME);
    QString varDirName = MSSettings::roSettings()->value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}
