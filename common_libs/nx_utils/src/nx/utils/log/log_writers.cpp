#include "log_writers.h"

#include <iostream>
#include <QtCore/QDir>
#include <QtCore/QFile>

namespace nx {
namespace utils {
namespace log {

void StdOut::write(const QString& message)
{
    std::cout << message.toStdString() << std::endl;
}

File::File(Settings settings):
    m_settings(std::move(settings))
{
}

void File::write(const QString& message)
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
    const auto currentPosition = m_file.tellp();
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

} // namespace log
} // namespace utils
} // namespace nx
