// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <QtCore/QDir>

#include <nx/utils/thread/long_runnable.h>

class QuaZip;

namespace nx::zip {

class NX_ZIP_API Extractor: public QnLongRunnable
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
        Busy, //< Extractor is already extracting some file.
    };
    Q_ENUM(Error)

    Extractor(const QString& fileName, const QDir& targetDir);
    virtual ~Extractor() override;

    static QString errorToString(Error error);

    Error error() const;
    QDir dir() const;

    qint64 estimateUnpackedSize() const;
    qint64 bytesExtracted() const;

    Error extractZip();
    QStringList fileList();

    Error tryOpen();
    void createEmptyZipFile();

signals:
    void finished(nx::zip::Extractor::Error error);

protected:
    virtual void run() override;

private:
    QDir m_dir;
    QScopedPointer<QuaZip> m_zip;
    std::atomic_int64_t m_extracted;
    Error m_lastError = Ok;
};

} // namespace nx::zip
