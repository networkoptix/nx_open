#pragma once

#include <future>

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
        Busy,       //< Extractor is already busy extracting some file.
    };

    QnZipExtractor(const QString &fileName, const QDir &targetDir);
    QnZipExtractor(QIODevice *ioDevice, const QDir &targetDir);

    QnZipExtractor();
    virtual ~QnZipExtractor() override;

    static QString errorToString(Error error);

    Error error() const;
    QDir dir() const;

    qint64 estimateUnpackedSize() const;
    qint64 bytesExtracted() const;

    Error extractZip();
    QStringList fileList();

signals:
    void finished(int error);

protected:
    virtual void run() override;

private:
    QDir m_dir;
    QScopedPointer<QuaZip> m_zip;
    std::atomic_int64_t m_extracted;
    Error m_lastError;
};
