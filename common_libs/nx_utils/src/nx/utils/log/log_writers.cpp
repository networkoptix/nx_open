#include "log_writers.h"

#include <iostream>
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
        return QString(kTemplate).arg(m_settings.name).arg(backupNumber, 3, QLatin1Char('0'));
    }
}

bool File::openFile()
{
    if (m_file.is_open())
        return true;

    #ifdef Q_OS_WIN
        const auto fileName = makeFileName().toStdWString();
    #else
        const auto fileName = makeFileName().toStdString();
    #endif

    m_file.open(fileName, std::ios_base::in | std::ios_base::out);
    if (!m_file.fail())
    {
        // File exists, prepare to append.
        m_file.seekp(std::ios_base::streamoff(0), std::ios_base::end);
        return !m_file.fail();
    }

    m_file.open(fileName, std::ios_base::out);
    if (m_file.fail())
        return false;

    // Ensure 1st char is UTF8 BOM.
    m_file.write("\xEF\xBB\xBF", 3);
    return !m_file.fail();
}

void File::rotateIfNeeded()
{
    if (m_file.tellp() < m_settings.size)
        return;

    m_file.close();
    if (QFile::exists(makeFileName(m_settings.count)))
    {
        // If all files used up we need to rotate all of them.
        for (size_t i = 2; i <= m_settings.size; ++i)
            QFile::rename(makeFileName(i), makeFileName(i - 1));

        QFile::rename(makeFileName(), makeFileName(m_settings.count));
    }
    else
    {
        // Otherwise just move current file to the first free place.
        size_t firstFreeNumber = 1;
        while (QFile::exists(makeFileName(firstFreeNumber)))
            ++firstFreeNumber;

        QFile::rename(makeFileName(), makeFileName(m_settings.count));
    }
}

} // namespace log
} // namespace utils
} // namespace nx
