#include "client_installation.h"

#include <QtCore/QDataStream>
#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/utils/log/log.h>
#include <applauncher_app_info.h>

namespace {

static const QString kInstallationDataFile("install.dat");

bool fillFileSizes(const QDir& root, const QString& base, QMap<QString, qint64>& fileSizeByEntry)
{
    QDir currentDir = root;
    currentDir.cd(base);
    for (const auto &info: currentDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
    {
        if (info.isDir())
        {
            if (!fillFileSizes(root, root.relativeFilePath(info.absoluteFilePath()), fileSizeByEntry))
                return false;
        }
        else
        {
            QFile file(info.absoluteFilePath());
            if (!file.open(QFile::ReadOnly))
                return false;
            fileSizeByEntry.insert(root.relativeFilePath(info.absoluteFilePath()), file.size());
            file.close();
        }
    }
    return true;
}

} // namespaces

bool QnClientInstallation::exists() const
{
    return !m_binaryPath.isEmpty() && QFile::exists(executableFilePath());
}

nx::utils::SoftwareVersion QnClientInstallation::version() const
{
    return m_version;
}

void QnClientInstallation::setVersion(const nx::utils::SoftwareVersion& version)
{
    m_version = version;
}

QString QnClientInstallation::rootPath() const
{
    return m_rootPath;
}

QString QnClientInstallation::binaryPath() const
{
    return QFileInfo(executableFilePath()).absolutePath();
}

QString QnClientInstallation::executableFilePath() const
{
    return m_binaryPath.isEmpty() ? QString() : m_rootPath + "/" + m_binaryPath;
}

QString QnClientInstallation::libraryPath() const
{
    return m_libPath.isEmpty() ? QString() : m_rootPath + "/" + m_libPath;
}

bool QnClientInstallation::verify() const
{
    NX_DEBUG(this, QString::fromLatin1("Entered VerifyInstallation"));

    QMap<QString, qint64> fileSizeByEntry;
    QDir rootDir(m_rootPath);

    {
        QFile file(rootDir.absoluteFilePath(kInstallationDataFile));
        if (!file.open(QFile::ReadOnly))
            return false;

        QDataStream stream(&file);
        stream >> fileSizeByEntry;
        file.close();
    }

    /* File is written after the successful end of the downloading, so it's presence is a marker.*/
    return !fileSizeByEntry.isEmpty();
}

QString targetBinaryPath(QString binaryPath)
{
#ifdef Q_OS_LINUX
    static const auto kPrefix = "bin/";
#elif defined Q_OS_MAC
    static const auto kPrefix = QnApplauncherAppInfo::bundleName() +  "/Contents/MacOS/";
#else
    static const auto kPrefix = "";
#endif
    return binaryPath.prepend(kPrefix);
}

QnClientInstallationPtr QnClientInstallation::installationForPath(const QString& rootPath)
{
    QDir rootDir(rootPath);

    QnClientInstallationPtr installation(new QnClientInstallation());
    installation->m_rootPath = rootPath;

    const QString binary = targetBinaryPath(QnApplauncherAppInfo::clientBinaryName());
    if (rootDir.exists(binary))
        installation->m_binaryPath = binary;
    else
        return QnClientInstallationPtr();

    QString lib = "lib";
    if (rootDir.exists(lib)) {
        installation->m_libPath = lib;
    } else {
        lib.prepend("../");
        if (rootDir.exists(lib))
            installation->m_libPath = lib;
    }

    return installation;
}

bool QnClientInstallation::createInstallationDat()
{
    QMap<QString, qint64> fileSizeByEntry;
    if (!fillFileSizes(QDir(rootPath()), QString(), fileSizeByEntry))
        return false;

    QFile file(rootPath() + "/" + kInstallationDataFile);
    if (!file.open(QFile::WriteOnly))
        return false;

    QDataStream stream(&file);
    stream << fileSizeByEntry;
    file.close();

    return true;
}

bool QnClientInstallation::isNeedsVerification() const
{
    return m_needsVerification;
}

void QnClientInstallation::setNeedsVerification(bool f)
{
    m_needsVerification = f;
}
