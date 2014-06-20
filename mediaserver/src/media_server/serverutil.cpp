#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>

#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
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

static bool useAlternativeGuid = false;
void setUseAlternativeGuid(bool value)
{
    useAlternativeGuid = value;
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

    QString name = useAlternativeGuid ? lit("serverGuid2") : lit("serverGuid");

    guid = MSSettings::roSettings()->value(name).toString();
    if (guid.isNull())
    {
        if (!MSSettings::roSettings()->isWritable())
        {
            return guid;
        }

#ifdef EDGE_SERVER
	char  mac[13];
	memset(mac, 0, sizeof(mac));
	char* host = 0;
	mac_eth0(mac, &host);
	QCryptographicHash md5Hash( QCryptographicHash::Md5 );
	md5Hash.addData(mac, 12);
	md5Hash.addData("edge");
	guid = QUuid::fromRfc4122(md5Hash.result()).toString();
#else
	guid = QUuid::createUuid();
#endif
        MSSettings::roSettings()->setValue(name, guid.toString());
    }
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


bool backupDatabase() {
    // TODO: #dklychkov dumpDatabaseAsync has not been implemented yet. Let this function work when dumpDatabaseAsync is ready.
    return true;

    QString dir = getDataDirectory() + lit("/");
    QString fileName;
    for (int i = -1; ; i++) {
        QString suffix = (i < 0) ? QString() : lit("_") + QString::number(i);
        fileName = dir + lit("ecs") + suffix + lit(".backup");
        if (!QFile::exists(fileName))
            break;
    }

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly))
        return false;

    QByteArray data;
    ec2::ErrorCode errorCode;

    QEventLoop loop;
    auto dumpDatabaseHandler =
        [&loop, &errorCode, &data] (int /*reqID*/, ec2::ErrorCode _errorCode, const QByteArray &dbData) {
            errorCode = _errorCode;
            data = dbData;
            loop.quit();
    };

    QnAppServerConnectionFactory::getConnection2()->dumpDatabaseAsync(&loop, dumpDatabaseHandler);
    loop.exec();

    if (errorCode != ec2::ErrorCode::ok) {
        NX_LOG(lit("Failed to dump EC database: %1").arg(ec2::toString(errorCode)), cl_logERROR);
        return false;
    }

    file.write(data);
    file.close();

    return true;
}
