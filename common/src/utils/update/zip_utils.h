#ifndef ZIP_UTILS_H
#define ZIP_UTILS_H

#include <QtCore/QIODevice>
#include <QtCore/QDir>

#include <nx/utils/thread/long_runnable.h>

class QuaZip;

class QnZipExtractor : public QnLongRunnable
{
    Q_OBJECT
public:
    enum Error
    {
        Ok,
        BrokenZip,
        WrongDir,
        CantOpenFile,
        NoFreeSpace,
        OtherError,
        Stopped,
        Busy,       //< Extractor is already busy extracting some file
    };

    QnZipExtractor(const QString &fileName, const QDir &targetDir);
    QnZipExtractor(QIODevice *ioDevice, const QDir &targetDir);

    QnZipExtractor();
    ~QnZipExtractor();

    /*
    Error extract(const QString &fileName, const QDir &targetDir);
    Error extract(QIODevice *ioDevice, const QDir &targetDir);
    */

    static QString errorToString(Error error);

    Error error() const;
    QDir dir() const;

    Error extractZip();
    QStringList fileList();

    std::shared_future<int> getFuture();

signals:
    void finished(int error);

protected:
    virtual void run() override;

private:
    QDir m_dir;
    QScopedPointer<QuaZip> m_zip;
    nx::utils::Mutex m_mutex;
    Error m_lastError;

    std::promise<int> m_result;
    std::shared_future<int> m_future;
};

#endif // ZIP_UTILS_H
