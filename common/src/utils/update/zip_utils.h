#ifndef ZIP_UTILS_H
#define ZIP_UTILS_H

#include <QtCore/QIODevice>
#include <QtCore/QDir>

#include <utils/common/long_runnable.h>

class QuaZip;

class QnZipExtractor : public QnLongRunnable {
    Q_OBJECT
public:
    enum Error {
        Ok,
        BrokenZip,
        WrongDir,
        CantOpenFile,
        NoFreeSpace,
        OtherError,
        Stopped
    };

    QnZipExtractor(const QString &fileName, const QDir &targetDir);
    QnZipExtractor(QIODevice *ioDevice, const QDir &targetDir);

    static QString errorToString(Error error);

    Error error() const;
    QDir dir() const;

signals:
    void finished(int error);

protected:
    virtual void run() override;

private:
    void finish(Error error);

private:
    QDir m_dir;
    QuaZip *m_zip;
    Error m_lastError;
};

#endif // ZIP_UTILS_H
