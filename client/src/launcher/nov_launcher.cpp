#include "nov_launcher.h"
#include "utils/common/util.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"

static const int IO_BUFFER_SIZE = 1024*1024;
static const qint64 MAGIC = 0x73a0b934820d4055ll;

QString getFullFileName(const QString& folder, const QString& fileName)
{
    if (folder.isEmpty())
        return fileName;

    QString value = folder;
    for (int i = 0; i < value.length(); ++i)
    {
        if (value[i] == L'\\')
            value[i] = L'/';
    }
    if (value[value.length()-1] != L'/' && value[value.length()-1] != L'\\')
        value += L'/';
    value += fileName;

    return value;
}

int QnNovLauncher::appendFile(QFile& dstFile, const QString& srcFileName)
{
    QFile srcFile(srcFileName);
    char* buffer = new char[IO_BUFFER_SIZE];
    try 
    {
        if (!srcFile.open(QIODevice::ReadOnly))
            return -2;

        int readed = srcFile.read(buffer, IO_BUFFER_SIZE);
        while (readed > 0)
        {
            dstFile.write(buffer, readed);
            readed = srcFile.read(buffer, IO_BUFFER_SIZE);
        }
        srcFile.close();
        delete [] buffer;
        return 0;
    }
    catch(...) {
        delete [] buffer;
        return -2;
    }
}

int QnNovLauncher::writeIndex(QFile& dstFile, const QVector<qint64>& filePosList, QVector<QString>& fileNameList)
{
    qint64 indexStartPos = dstFile.pos();
    for (int i = 1; i < filePosList.size(); ++i)
    {
        dstFile.write((const char*) &filePosList[i-1], sizeof(qint64)); // no marsahling is required because of platform depending executable file
        int strLen = fileNameList[i].length();
        dstFile.write((const char*) &strLen, sizeof(int));
        dstFile.write((const char*) fileNameList[i].data(), strLen*sizeof(wchar_t));
    }

    //dstFile.write((const char*) &MAGIC, sizeof(qint64)); // nov file posf
    //dstFile.write((const char*) &filePosList[filePosList.size()-2], sizeof(qint64)); // nov file start
    dstFile.write((const char*) &indexStartPos, sizeof(qint64));

    return 0;
}

void QnNovLauncher::getSrcFileList(QVector<QString>& result, const QString& srcDataFolder, const QString& root)
{
    QDir dir(srcDataFolder);
    QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    for (int i = 0; i < list.size(); ++i)
    {
        QString name = list[i].fileName();
        if (list[i].isDir())
            getSrcFileList(result, getFullFileName(srcDataFolder, name), closeDirPath(getFullFileName(root, name)));
        else 
            result.push_back(root + name);
    }
}

int QnNovLauncher::createLaunchingFile(const QString& dstName, const QString& novFileName)
{
    QSet<QString> allowedFileExt;
    allowedFileExt << QLatin1String(".exe");
    allowedFileExt << QLatin1String(".dll");


    QString srcDataFolder = QFileInfo(qApp->arguments()[0]).path();
    QString launcherFile(QLatin1String(":/launcher.exe"));

    QVector<qint64> filePosList;
    QVector<QString> fileNameList;
    QVector<QString> srcMediaFiles;
    getSrcFileList(srcMediaFiles, srcDataFolder, QString());

    QFile dstFile(dstName);
    if (!dstFile.open(QIODevice::WriteOnly))
        return -1;

    if (appendFile(dstFile, launcherFile) != 0)
        return -2;
    filePosList.push_back(dstFile.pos());
    fileNameList.push_back(launcherFile);

    for (int i= 0; i < srcMediaFiles.size(); ++i)
    {
        if (allowedFileExt.contains(srcMediaFiles[i].right(4).toLower()))
        {
            if (appendFile(dstFile, getFullFileName(srcDataFolder, srcMediaFiles[i])) == 0)
            {
                filePosList.push_back(dstFile.pos());
                fileNameList.push_back(srcMediaFiles[i]);
            }
        }
    }
    
    filePosList.push_back(dstFile.pos());
    fileNameList.push_back(novFileName);

    if (writeIndex(dstFile, filePosList, fileNameList) != 0)
        return -4;

    qint64 novPos = dstFile.pos();
    if (novFileName.isEmpty()) {
        QnLayoutFileStorageResource::QnLayoutFileIndex idex;
        dstFile.write((const char*) &idex, sizeof(idex)); // nov file start
    }
    else {
        if (appendFile(dstFile, novFileName) != 0)
            return -3;
    }

    dstFile.write((const char*) &novPos, sizeof(qint64)); // nov file start
    dstFile.write((const char*) &MAGIC, sizeof(qint64)); // magic

    dstFile.close();
    return 0;
}
