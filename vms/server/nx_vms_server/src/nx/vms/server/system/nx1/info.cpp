#include "info.h"

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <array>

#include <QtCore/QFile>
#include <QtCore/QTextStream>

#include "utils/common/app_info.h"
#include <nx/vms/server/fs/partitions/partitions_information_provider_linux.h>
#include <nx/vms/server/fs/partitions/read_partitions_linux.h>
#include <media_server/media_server_module.h>
#include <nx/network/nettools.h>
#include <nx/vms/server/root_fs.h>

namespace
{

static QString readFile(const QString& file)
{
    QString result;

    QFile f(file);
    if (!f.open(QFile::ReadOnly | QFile::Text))
        return result;

    QTextStream in(&f);
    result = in.readAll();
    f.close();

    return result.trimmed();
}

} // namespace

QString Nx1::getMac()
{
    return nx::network::getMacFromPrimaryIF().replace(QLatin1Char('-'), QLatin1Char(':'));
}

QString Nx1::getSerial()
{
    return readFile("/tmp/serial");
}

bool Nx1::isBootedFromSD(QnMediaServerModule* serverModule)
{
    // On Nx1 in both modes (recovery and normal) both `mount` and `cat /proc/mounts` commands
    // shows the root device partition as "/dev/root". Further, /dev/root always is a link to
    // an SD card partition in case of recovery mode or to a hard disk partition in case of normal
    // mode. In recovery mode the system is booted from SD card. In normal mode - from hard disk.
    // Thus the direction of this link is used to determine the current mode and the source of the
    // boot.

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    namespace fs = nx::vms::server::fs;

    fs::PartitionsInformationProvider partitionsInfoProvider(serverModule->globalSettings(), serverModule->rootFileSystem());
    std::list<fs::PartitionInfo> partitionInfoList;

    if (fs::readPartitionsInformation(&partitionsInfoProvider, &partitionInfoList)
        != SystemError::noError)
    {
        return true;
    }

    for (const auto& partitionInfo: partitionInfoList)
    {
        if (partitionInfo.path == "/")
        {
            if (partitionInfo.devName.indexOf("rootfs") != -1 ||
                partitionInfo.devName == "/dev/root")
            {
                struct stat st;
                memset(&st, 0, sizeof(st));
                if (stat("/dev/root", &st) == -1)
                {
                    // if /dev/root is unavailable, considering
                    return false;
                }

                if (!(st.st_mode & S_IFLNK))
                    return true; //OS is on SD card or some internal memory

                //checking that root is really on SD card
                std::array<char, PATH_MAX> realPath;
                const auto realPathLen = readlink("/dev/root", realPath.data(), realPath.size());
                if (realPathLen == -1)
                {
                    //failure
                    return false;
                }

                if (QString::fromLocal8Bit(realPath.data(), realPathLen).indexOf("sd") != -1)
                    return false;  //OS is on regular drive. Can change root password

                return true; //OS is on SD card or some internal memory
            }
            else
            {
                return false;
            }
        }
    }

    return false;
#else
    return false;
#endif
}
