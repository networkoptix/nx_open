#ifndef __NOV_LAUNCHER_H__
#define __NOV_LAUNCHER_H__

class QnNovLauncher
{
public:
    static int createLaunchingFile(const QString& dstName, const QString& novFileName = QString());
private:
    static int appendFile(QFile& dstFile, const QString& srcFileName);
    static int writeIndex(QFile& dstFile, const QVector<qint64>& filePosList, QVector<QString>& fileNameList);
    static void getSrcFileList(QVector<QString>& result, const QString& srcDataFolder, const QString& root);
};

#endif // __NOV_LAUNCHER_H__