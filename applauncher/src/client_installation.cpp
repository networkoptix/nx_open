#include "client_installation.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

#include "utils/common/log.h"
#include "version.h"

namespace {
    const QString installationDataFile("install.dat");

    bool fillFileSizes(const QDir &root, const QString &base, QMap<QString, qint64> &fileSizeByEntry) {
        QDir currentDir = root;
        currentDir.cd(base);
        foreach (const QFileInfo &info, currentDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
            if (info.isDir()) {
                if (!fillFileSizes(root, root.relativeFilePath(info.absoluteFilePath()), fileSizeByEntry))
                    return false;
            } else {
                QFile file(info.absoluteFilePath());
                if (!file.open(QFile::ReadOnly))
                    return false;
                fileSizeByEntry.insert(root.relativeFilePath(info.absoluteFilePath()), file.size());
                file.close();
            }
        }
        return true;
    }
}

QnClientInstallation::QnClientInstallation() : m_needsVerification(true) {}

bool QnClientInstallation::exists() const {
    return !m_binaryPath.isEmpty() && QFile::exists(executableFilePath());
}

QnSoftwareVersion QnClientInstallation::version() const {
    return m_version;
}

void QnClientInstallation::setVersion(const QnSoftwareVersion &version) {
    m_version = version;
}

QString QnClientInstallation::rootPath() const {
    return m_rootPath;
}

QString QnClientInstallation::binaryPath() const {
    return QFileInfo(executableFilePath()).absolutePath();
}

QString QnClientInstallation::executableFilePath() const {
    return m_binaryPath.isEmpty() ? QString() : m_rootPath + "/" + m_binaryPath;
}

QString QnClientInstallation::libraryPath() const {
    return m_libPath.isEmpty() ? QString() : m_rootPath + "/" + m_libPath;
}

bool QnClientInstallation::verify() const {
    NX_LOG(QString::fromLatin1("Entered VerifyInstallation"), cl_logDEBUG1);

    QMap<QString, qint64> fileSizeByEntry;
    QDir rootDir(m_rootPath);

    {
        QFile file(rootDir.absoluteFilePath(installationDataFile));
        if (!file.open(QFile::ReadOnly))
            return false;

        QDataStream stream(&file);
        stream >> fileSizeByEntry;
        file.close();
    }

    if (fileSizeByEntry.isEmpty())
        return false;

    for (auto it = fileSizeByEntry.begin(); it != fileSizeByEntry.end(); ++it) {
        if (QFile(rootDir.absoluteFilePath(it.key())).size() != it.value())
            return false;
    }

    return true;
}

QnClientInstallationPtr QnClientInstallation::installationForPath(const QString &rootPath) {
    QDir rootDir(rootPath);

    QnClientInstallationPtr installation(new QnClientInstallation());
    installation->m_rootPath = rootPath;

    QString binary = QN_CLIENT_EXECUTABLE_NAME;
    if (rootDir.exists(binary)) {
        installation->m_binaryPath = binary;
    } else {
        binary.prepend("bin/");
        if (rootDir.exists(binary))
            installation->m_binaryPath = binary;
        else
            return QnClientInstallationPtr();
    }

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

bool QnClientInstallation::createInstallationDat() {
    QMap<QString, qint64> fileSizeByEntry;
    if (!fillFileSizes(QDir(rootPath()), QString(), fileSizeByEntry))
        return false;

    QFile file(rootPath() + "/" + installationDataFile);
    if (!file.open(QFile::WriteOnly))
        return false;

    QDataStream stream(&file);
    stream << fileSizeByEntry;
    file.close();

    return true;
}

bool QnClientInstallation::isNeedsVerification() const {
    return m_needsVerification;
}

void QnClientInstallation::setNeedsVerification(bool f) {
    m_needsVerification = f;
}
