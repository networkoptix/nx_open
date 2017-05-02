#include "log_writers.h"

#include <iostream>
#include <QtCore/QDir>
#include <QtCore/QFile>

#include "to_string.h"

namespace nx {
namespace utils {
namespace log {

Level levelFromString(const QString& levelString)
{
    const auto level = levelString.toLower();
    if (level == QLatin1String("none"))
        return Level::none;

    if (level == QLatin1String("always"))
        return Level::always;

    if (level == QLatin1String("error"))
        return Level::error;

    if (level == QLatin1String("warning"))
        return Level::warning;

    if (level == QLatin1String("info"))
        return Level::info;

    if (level == QLatin1String("debug") || level == QLatin1String("debug1"))
        return Level::debug;

    if (level == QLatin1String("verbose") || level == QLatin1String("debug2"))
        return Level::verbose;

    return Level::undefined;
}

QString toString(Level level)
{
    switch (level)
    {
        case Level::undefined:
            return QLatin1String("undefined");

        case Level::none:
            return QLatin1String("none");

        case Level::always:
            return QLatin1String("always");

        case Level::error:
            return QLatin1String("error");

        case Level::warning:
            return QLatin1String("warning");

        case Level::info:
            return QLatin1String("info");

        case Level::debug:
            return QLatin1String("debug");

        case Level::verbose:
            return QLatin1String("verbose");
    };

    NX_ASSERT(false, lm("Unknown level: %1").arg(static_cast<int>(level)));
    return QLatin1String("unknown");
}

void StdOut::write(Level level, const QString& message)
{
    writeImpl(level, message);
}

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    void StdOut::writeImpl(Level level, const QString& message)
    {
        switch (level)
        {
            case Level::error:
            case Level::warning:
                std::cerr << message.toStdString() << std::endl;
                break;

            default:
                std::cout << message.toStdString() << std::endl;
                break;
        }
    }
#endif

File::File(Settings settings):
    m_settings(std::move(settings))
{
    std::cout 
        << ::toString(this).toStdString() << ": " 
        << makeFileName().toStdString() << std::endl;
}

void File::write(Level /*level*/, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    if (!openFile())
    {
        std::cout << message.toStdString() << std::endl;
        return;
    }

    m_file << message.toStdString() << std::endl;
    rotateIfNeeded();
}

QString File::makeFileName(size_t backupNumber) const
{
    if (backupNumber == 0)
    {
        static const QLatin1String kTemplate("%1.log");
        return QString(kTemplate).arg(m_settings.name);
    }
    else
    {
        static const QLatin1String kTemplate("%1_%2.log");
        return QString(kTemplate).arg(m_settings.name).arg(
            (int) backupNumber, 3, 10, QLatin1Char('0'));
    }
}

bool File::openFile()
{
    if (m_file.is_open())
        return true;

    const auto fileNameQString = makeFileName();
    #ifdef Q_OS_WIN
        const auto fileName = fileNameQString.toStdWString();
    #else
        const auto fileName = fileNameQString.toStdString();
    #endif

    m_file.open(fileName, std::ios_base::in | std::ios_base::out);
    if (!m_file.fail())
    {
        // File exists, prepare to append.
        m_file.seekp(std::ios_base::streamoff(0), std::ios_base::end);
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
    QnMutexLocker lock(&m_mutex);
    m_messages.push_back(message);
}

std::vector<QString> Buffer::take()
{
    QnMutexLocker lock(&m_mutex);
    std::vector<QString> messages;
    std::swap(m_messages, messages);
    return messages;
}

} // namespace log
} // namespace utils
} // namespace nx
