#include "nov_launcher_win.h"

#include <vector>

#include <client/client_app_info.h>
#include <core/storage/file_storage/layout_storage_resource.h>

namespace {

static const int IO_BUFFER_SIZE = 1024 * 1024;

// TODO: #GDM #low move out to common place with launcher.exe
static const qint64 MAGIC = 0x73a0b934820d4055ll;

struct FileDescriptor
{
    FileDescriptor(const QString& filename, qint64 position):
        filename(filename), position(position)
    {
    }

    QString filename;
    qint64 position = 0;
};

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

bool writeIndex(QFile& dstFile, const std::vector<FileDescriptor>& files)
{
    const qint64 indexStartPos = dstFile.pos();
    NX_VERBOSE(typeid(QnNovLauncher), "Index starts at %1", indexStartPos);
    for (size_t i = 1; i < files.size(); ++i)
    {
        // Reading file start position as the end of the previous file.
        const qint64 filePosition = files[i - 1].position;
        NX_VERBOSE(typeid(QnNovLauncher), "File %1 starts at %2",
            files[i].filename,
            filePosition);

        // No marshaling is required because of platform depending executable file.
        dstFile.write((const char*)&filePosition, sizeof(qint64));
        int strLen = files[i].filename.length();
        dstFile.write((const char*)&strLen, sizeof(int));
        dstFile.write((const char*)files[i].filename.data(), strLen * sizeof(wchar_t));
    }

    const qint64 indexPtrPos = dstFile.pos();
    NX_VERBOSE(typeid(QnNovLauncher), "Index ptr is located at %1", indexPtrPos);
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
    static const QStringList kNameFilters{
        "*.exe",
        "*.dll",
        "*.conf"
    };

    static const QStringList kExtraDirs{
        "vox",
        "fonts",
        "qml",
        "help",
        "resources",
    };

    static const QStringList kIgnoredFiles{
#ifdef _DEBUG
        "mediaserver.exe",
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

} // namespace

// TODO: Sync with nov_file_launcher sources.
/**
 * Packed file structure:
 * /--------------------/
 * / launcher.exe       /
 * /--------------------/
 * / library1.dll       /
 * / library2.dll       /
 * / ...                /
 * / HD Witness.exe     /
 * / ...                /
 * / misc client files  /
 * /--------------------/
 * / INDEX table        /
 * / INDEX PTR: 8 bytes /
 * /--------------------/
 * / NOV file           /
 * / NOV PTR: 8 bytes   /
 * /--------------------/
 * / MAGIC: 8 bytes     /
 * /--------------------/
 */
QnNovLauncher::ErrorCode QnNovLauncher::createLaunchingFile(
    const QString& dstName,
    ExportMode exportMode)
{
    static const QString kLauncherFile(":/launcher.exe");

    QDir sourceRoot = qApp->applicationDirPath();

    /* List of client binaries. */
    auto sourceFiles = calculateFileList(sourceRoot);

    std::vector<FileDescriptor> files;
    files.reserve(1000);

    QFile dstFile(dstName);
    if (!dstFile.open(QIODevice::WriteOnly))
        return ErrorCode::NoTargetFileAccess;

    if (!appendFile(dstFile, kLauncherFile))
        return ErrorCode::WriteFileError;

    files.emplace_back(kLauncherFile, dstFile.pos());

    for (const auto& absolutePath: sourceFiles)
    {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents); // A bit risky but probably OK.

        const QString relativePath = sourceRoot.relativeFilePath(absolutePath);
        if (!appendFile(dstFile, absolutePath))
            return ErrorCode::WriteFileError;

        files.emplace_back(relativePath, dstFile.pos());
    }

    if (!writeIndex(dstFile, files))
        return ErrorCode::WriteIndexError;

    // Position where video data will start.
    const qint64 novPos = dstFile.pos();
    NX_VERBOSE(typeid(QnNovLauncher), "Nov file is located at %1", novPos);

    if (exportMode == ExportMode::standaloneClient)
    {
        dstFile.write((const char*)&novPos, sizeof(qint64)); // nov file start
        dstFile.write((const char*)&MAGIC, sizeof(qint64)); // magic
    }

    return ErrorCode::Ok;
}
