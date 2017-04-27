#pragma once

#include <fstream>
#include <QtCore/QString>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API AbstractWriter
{
public:
    virtual void write(const QString& message) = 0;
};

/**
 * Writes to std out.
 */
class NX_UTILS_API StdOut: public AbstractWriter
{
public:
    virtual void write(const QString& message) override;
};

/**
 * File writer with backup rotation.
 */
class NX_UTILS_API File: public AbstractWriter
{
public:
    struct Settings
    {
        QString name; /**< Base file name with path. */
        size_t size = 0; /**< Maximum file size. */
        size_t count = 0; /**< Maximum backup files count. */
    };

    File(Settings settings);
    virtual void write(const QString& message) override;

private:
    QString makeFileName(size_t backupNumber = 0) const;
    bool openFile();
    void rotateIfNeeded();

private:
    const Settings m_settings;
    QnMutex m_mutex;
    std::fstream m_file;
};

/**
 * Writes messages to internal buffer.
 */
class NX_UTILS_API Buffer: public AbstractWriter
{
public:
    virtual void write(const QString& message) override;
    std::vector<QString> take();

private:
    QnMutex m_mutex;
    std::vector<QString> m_messages;
};

} // namespace log
} // namespace utils
} // namespace nx
