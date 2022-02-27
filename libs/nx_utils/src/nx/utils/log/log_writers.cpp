// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_writers.h"

#include <iostream>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include "to_string.h"

namespace nx {
namespace utils {
namespace log {

void StdOut::write(Level level, const QString& message)
{
    writeImpl(level, message);
}

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

// For Android and iOS, this method is defined in dedicated files.
void StdOut::writeImpl(Level level, const QString& message)
{
    switch (level)
    {
        case Level::error:
        case Level::warning:
            std::cerr << message.toStdString() + '\n';
            break;

        default:
            std::cout << message.toStdString() + '\n';
            std::cout.flush();
            break;
    }
}

#endif // !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

File::File(Settings settings):
    m_settings(std::move(settings))
{
}

void File::write(Level /*level*/, const QString& message)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!openFile())
    {
        std::cerr << message.toStdString() + '\n';
        return;
    }

    m_file << message.toStdString() << std::endl;
    rotateIfNeeded();
}

QString File::makeFileName(size_t backupNumber) const
{
    static constexpr char kExtensionWithSeparator[] = ".log";

    auto baseFileName = m_settings.name;
    if (baseFileName.endsWith(kExtensionWithSeparator))
        baseFileName.chop(strlen(kExtensionWithSeparator));

    if (backupNumber == 0)
    {
        static const QLatin1String kTemplate("%1.log");
        return QString(kTemplate).arg(baseFileName);
    }
    else
    {
        static const QLatin1String kTemplate("%1_%2.log");
        return QString(kTemplate).arg(baseFileName).arg(
            (int) backupNumber, 3, 10, QLatin1Char('0'));
    }
}

bool File::openFile()
{
    if (m_file.is_open())
        return true;

    std::cerr << nx::toString(this).toStdString() + ": " + makeFileName().toStdString() + '\n';

    const auto fileNameQString = makeFileName();
    #if defined(Q_OS_WIN)
        const auto fileName = fileNameQString.toStdWString();
    #else
        const auto fileName = fileNameQString.toStdString();
    #endif

    m_file.open(fileName, std::ios_base::in | std::ios_base::out);
    if (!m_file.fail())
    {
        // File exists, prepare to append.
        m_file.seekp(std::streamoff(0), std::ios_base::end);
        return !m_file.fail();
    }

    const auto directory = QFileInfo(fileNameQString).absoluteDir();
    if (!directory.exists())
        directory.mkpath(QLatin1String("."));

    m_file.open(fileName, std::ios_base::out);
    if (m_file.fail())
        return false;

    // Ensure 1st char is UTF8 BOM.
    m_file.write("\xEF\xBB\xBF", 3);
    return !m_file.fail();
}

void File::rotateIfNeeded()
{
    const auto currentPosition = (size_t) m_file.tellp();
    if (currentPosition < m_settings.size)
        return;

    m_file.close();
    if (m_settings.count == 0)
    {
        QFile::remove(makeFileName());
        return;
    }

    if (QFile::exists(makeFileName(m_settings.count)))
    {
        // If all files used up we need to rotate all of them.
        QFile::remove(makeFileName(1));
        for (size_t i = 2; i <= m_settings.count; ++i)
            QFile::rename(makeFileName(i), makeFileName(i - 1));

        QFile::rename(makeFileName(), makeFileName(m_settings.count));
    }
    else
    {
        // Otherwise just move current file to the first free place.
        size_t firstFreeNumber = 1;
        while (QFile::exists(makeFileName(firstFreeNumber)))
            ++firstFreeNumber;

        QFile::rename(makeFileName(), makeFileName(firstFreeNumber));
    }
}

void Buffer::write(Level /*level*/, const QString& message)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_messages.push_back(message);
}

std::vector<QString> Buffer::takeMessages()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    std::vector<QString> messages;
    std::swap(m_messages, messages);
    return messages;
}

void Buffer::clear()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_messages.clear();
}

void NullDevice::write(Level /*level*/, const QString& /*message*/)
{
}

} // namespace log
} // namespace utils
} // namespace nx
