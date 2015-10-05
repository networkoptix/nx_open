#include "info.h"

#include <array>

#include <QtCore/QFile>
#include <QtCore/QTextStream>

#include "utils/fs/dir.h"
#include "utils/network/nettools.h"


namespace
{
    QString readFile(const QString& file)
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
}

QString Nx1::getMac()
{
    return getMacFromPrimaryIF().replace(QLatin1Char('-'), QLatin1Char(':'));
}

QString Nx1::getSerial()
{
    return readFile(lit("/tmp/serial"));
}

bool Nx1::isBootedFromSD()
{
#ifdef __linux__
    if (QnAppInfo::armBox() == "bpi" || QnAppInfo::armBox() == "nx1")
    {
        std::list<PartitionInfo> partitionInfoList;
        if (!readPartitions(&partitionInfoList))
            return true;

        for (const auto& partitionInfo: partitionInfoList)
        {
            if (partitionInfo.path == "/")
            {
                if (partitionInfo.devName.indexOf(lit("rootfs")) != -1 ||
                    partitionInfo.devName == lit("/dev/root"))
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

                    if (QString::fromLocal8Bit(realPath.data(), realPathLen).indexOf(lit("sd")) != -1)
                        return false;  //OS is on regular drive. Can change root password

                    return true; //OS is on SD card or some internal memory
                }
                else
                {
                    return false;
                }
            }
        }
    }

    return false;
#else
    return false;
#endif
}
