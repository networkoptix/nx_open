// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <fstream>
#include <nx/utils/thread/mutex.h>
#include "log_level.h"

namespace nx {
namespace utils {
namespace log {

static constexpr int kDefaultLogArchiveCount = 25;
static constexpr int kDefaultMaxLogFileSize = 10 * 1024 * 1024;

class NX_UTILS_API AbstractWriter
{
public:
    virtual ~AbstractWriter() = default;
    virtual void write(Level level, const QString& message) = 0;
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
};

/**
 * File writer with backup rotation.
 */
class NX_UTILS_API File: public AbstractWriter
{
public:
    static QString makeFileName(QString fileName, size_t backupNumber);

    struct Settings
    {
        QString name; /**< Base file name with path. */
        size_t size = kDefaultMaxLogFileSize; /**< Maximum file size. */
        size_t count = kDefaultLogArchiveCount; /**< Maximum backup files count. */
    };

    File(Settings settings);
    virtual void write(Level level, const QString& message) override;
    QString getFileName(size_t backupNumber = 0) const;

    void setSettings(const Settings& settings);

private:
    bool openFile();
    void rotateIfNeeded();

private:
    Settings m_settings;
    nx::Mutex m_mutex;
    std::fstream m_file;
};

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

} // namespace log
} // namespace utils
} // namespace nx
