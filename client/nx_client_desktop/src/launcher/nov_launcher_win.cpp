#include "nov_launcher_win.h"

#include <client/client_app_info.h>

#include "utils/common/util.h"
#include "core/storage/file_storage/layout_storage_resource.h"

namespace {

static const int IO_BUFFER_SIZE = 1024 * 1024;

// TODO: #GDM #low move out to common place with launcher.exe
static const qint64 MAGIC = 0x73a0b934820d4055ll;

bool appendFile(QFile& dstFile, const QString& srcFileName)
{
    QFile srcFile(srcFileName);
    char* buffer = new char[IO_BUFFER_SIZE];
    try
    {
        if (!srcFile.open(QIODevice::ReadOnly))
            return false;

        int read = srcFile.read(buffer, IO_BUFFER_SIZE);
        while (read > 0)
        {
            if (dstFile.write(buffer, read) != read)
            {
                srcFile.close();
                delete[] buffer;
                return false;
            }

            read = srcFile.read(buffer, IO_BUFFER_SIZE);
        }
        srcFile.close();
        delete[] buffer;
        return true;
    }
    catch (...)
    {
        delete[] buffer;
        return false;
    }
}

bool writeIndex(QFile& dstFile, const QVector<qint64>& filePosList, QVector<QString>& fileNameList)
{
    qint64 indexStartPos = dstFile.pos();
    for (int i = 1; i < filePosList.size(); ++i)
    {
        // no marshaling is required because of platform depending executable file
        dstFile.write((const char*)&filePosList[i - 1], sizeof(qint64));
        int strLen = fileNameList[i].length();
        dstFile.write((const char*)&strLen, sizeof(int));
        dstFile.write((const char*)fileNameList[i].data(), strLen * sizeof(wchar_t));
    }

    dstFile.write((const char*)&indexStartPos, sizeof(qint64));

    return true;
}

void populateFileListRecursive(QSet<QString>& result, const QDir& dir,
    const QStringList& nameFilters = QStringList())
{
    QFileInfoList files = dir.entryInfoList(nameFilters, QDir::NoDotAndDotDot | QDir::Files);
    for (auto info : files)
        result << info.absoluteFilePath();

    QFileInfoList subDirs = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
    for (auto info : subDirs)
        populateFileListRecursive(result, info.absoluteFilePath(), nameFilters);
}

/* Returns set of absolute file paths. */
QSet<QString> calculateFileList(const QDir& sourceRoot)
{
    static const QStringList kNameFilters{lit("*.exe"), lit("*.dll")};
    static const QStringList kExtraDirs{
        lit("vox"), 
        lit("fonts"), 
        lit("qml"),
        lit("help")};

    static const QStringList kIgnoredFiles{
#ifdef _DEBUG
        lit("mediaserver.exe"),
#endif
        QnClientAppInfo::applauncherBinaryName(),
        QnClientAppInfo::minilauncherBinaryName()
    };

    QSet<QString> sourceFiles;
    populateFileListRecursive(sourceFiles, sourceRoot, kNameFilters);
    for (const auto& extraDir : kExtraDirs)
    {
        QDir d(sourceRoot.absoluteFilePath(extraDir));
        populateFileListRecursive(sourceFiles, d);
    }

    for (const auto& ignored : kIgnoredFiles)
        sourceFiles.remove(sourceRoot.absoluteFilePath(ignored));

    return sourceFiles;
}

}

QnNovLauncher::ErrorCode QnNovLauncher::createLaunchingFile(const QString& dstName, const QString& novFileName)
{
    static const QString kLauncherFile(lit(":/launcher.exe"));

    QDir sourceRoot = qApp->applicationDirPath();

    /* List of client binaries. */
    auto sourceFiles = calculateFileList(sourceRoot);

    QVector<qint64> filePosList;
    QVector<QString> fileNameList;

    QFile dstFile(dstName);
    if (!dstFile.open(QIODevice::WriteOnly))
        return ErrorCode::NoTargetFileAccess;

    if (!appendFile(dstFile, kLauncherFile))
        return ErrorCode::WriteFileError;

    filePosList.push_back(dstFile.pos());
    fileNameList.push_back(kLauncherFile);

    for (const auto& absolutePath : sourceFiles)
    {
        const QString relativePath = sourceRoot.relativeFilePath(absolutePath);
        if (!appendFile(dstFile, absolutePath))
            return ErrorCode::WriteFileError;

        filePosList.push_back(dstFile.pos());
        fileNameList.push_back(relativePath);
    }

    filePosList.push_back(dstFile.pos());
    fileNameList.push_back(novFileName);

    if (!writeIndex(dstFile, filePosList, fileNameList))
        return ErrorCode::WriteIndexError;

    qint64 novPos = dstFile.pos();
    if (novFileName.isEmpty())
    {
        QnLayoutFileStorageResource::QnLayoutFileIndex idex;
        dstFile.write((const char*)&idex, sizeof(idex)); // nov file start
    }
    else
    {
        if (!appendFile(dstFile, novFileName))
            return ErrorCode::WriteMediaError;
    }

    dstFile.write((const char*)&novPos, sizeof(qint64)); // nov file start
    dstFile.write((const char*)&MAGIC, sizeof(qint64)); // magic

    dstFile.close();

    return ErrorCode::Ok;
}
