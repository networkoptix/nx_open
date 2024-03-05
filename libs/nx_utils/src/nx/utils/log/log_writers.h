// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <fstream>
#include <future>

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QLockFile>

#include <nx/utils/log/log_level.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>

namespace nx::log {

static constexpr int kMaxLogRotation = 999;
static constexpr qint64 kDefaultMaxLogVolumeSizeB = 250 * 1024 * 1024;
static constexpr qint64 kDefaultMaxLogFileSizeB = 10 * 1024 * 1024;
static constexpr std::chrono::seconds kDefaultMaxLogFileTimePeriodS = std::chrono::seconds::zero();
static constexpr bool kDefaultLogArchivingEnabled = true;

class NX_UTILS_API AbstractWriter
{
public:
    virtual ~AbstractWriter() = default;
    virtual void write(Level level, const QString& message) = 0;
    virtual cf::future<cf::unit> stopArchivingAsync();
};

/**
 * Writes to std out.
 */
class NX_UTILS_API StdOut: public AbstractWriter
{
public:
    virtual void write(Level level, const QString& message) override;

private:
    static void writeImpl(Level level, const QString& message);

    nx::Mutex m_mutex;
};

/**
 * File writer with backup rotation.
 */
class NX_UTILS_API File: public AbstractWriter
{
public:
    enum Extension
    {
        log,
        tmp,
        zip,
        zipTmp,
    };

    template<typename Visitor>
    friend constexpr auto nxReflectVisitAllEnumItems(Extension*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<Extension>;
        return visitor(
            Item{log, ".log"},
            Item{tmp, ".log.tmp"},
            Item{zip, ".log.zip"},
            Item{zipTmp, ".log.zip.tmp"}
        );
    }

    static QString makeFileName(QString fileName, size_t backupNumber, Extension ext);
    static QString makeBaseFileName(QString path);

    struct Settings
    {
        QString name; /**< Base file name with path. */
        qint64 maxVolumeSizeB = kDefaultMaxLogVolumeSizeB; /**< Maximum volume size. */
        qint64 maxFileSizeB = kDefaultMaxLogFileSizeB; /**< Maximum file size. */
        std::chrono::seconds maxFileTimePeriodS = kDefaultMaxLogFileTimePeriodS; /**< Maximum file duration in time. */
        bool archivingEnabled = kDefaultLogArchivingEnabled; /**< Zipping enabled/disabled flag. */
    };

    File(Settings settings);
    virtual ~File();
    virtual void write(Level level, const QString& message) override;
    virtual cf::future<cf::unit> stopArchivingAsync() override;
    QString getFileName(size_t backupNumber = 0) const;

    void setSettings(const Settings& settings);

private:
    bool openFile();
    void rotateIfNeeded(nx::Locker<nx::Mutex>* lock);
    void rotateAndStartArchivingIfNeeded();
    void rotateLeftovers();
    bool queueToArchive(nx::Locker<nx::Mutex>* lock);
    bool isCurrentLimitReached(nx::Locker<nx::Mutex>* lock);
    void archive(QString fileName, QString archiveName);
    qint64 totalVolumeSize();

private:
    Settings m_settings;
    nx::Mutex m_mutex;
    std::fstream m_file;
    QFileInfo m_fileInfo;
    qint64 m_currentPos = 0;
    QDateTime m_fileOpenTime;
    QLockFile m_volumeLock;
    std::future<void> m_archive;
    int m_archiveQueue = 0;
};

NX_UTILS_API QString toQString(File::Extension ext);

/**
 * Writes messages to internal buffer.
 */
class NX_UTILS_API Buffer: public AbstractWriter
{
public:
    virtual void write(Level level, const QString& message) override;

    std::vector<QString> takeMessages();
    void clear();

private:
    nx::Mutex m_mutex;
    std::vector<QString> m_messages;
};

/**
 * Does nothing, all messages are lost.
 */
class NX_UTILS_API NullDevice: public AbstractWriter
{
public:
    virtual void write(Level level, const QString& message) override;
};

} // namespace nx::log
