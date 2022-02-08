// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_writers.h"

#include <iostream>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryFile>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include "to_string.h"

static constexpr int kBufferSize = 64 * 1024;

namespace nx {
namespace utils {
namespace log {

void StdOut::write(Level level, const QString& message)
{
    writeImpl(level, message);
}

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

// For Android and iOS, this method is defined in dedicated files.
void StdOut::writeImpl(Level /*level*/, const QString& message)
{
    std::cerr << message.toStdString() + '\n';
}

#endif // !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

File::File(Settings settings):
    m_settings(std::move(settings))
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    archiveLeftOvers(&lock);
}

void File::setSettings(const Settings& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    // The filename should not be changed, too dangerous.
    m_settings.size = settings.size;
    m_settings.count = settings.count;
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
    rotateIfNeeded(&lock);
}

QString File::makeFileName(QString fileName, size_t backupNumber)
{
    auto baseFileName = fileName;
    if (baseFileName.endsWith(kExtensionWithSeparator))
        baseFileName.chop(strlen(kExtensionWithSeparator));

    if (backupNumber == 0)
    {
        static const QString kTemplate("%1%2");
        return kTemplate.arg(baseFileName, kExtensionWithSeparator);
    }
    else
    {
        static const QString kTemplate("%1_%2%3");
        return kTemplate.arg(baseFileName).arg((int) backupNumber, 3, 10, QLatin1Char('0'))
            .arg(kRotateExtensionWithSeparator);
    }
}

QString File::makeBaseFileName(QString fullPath)
{
    // Remove the path and any log-related ending.
    auto name = QFileInfo(fullPath).baseName();

    // Remove the rotation counter if any.
    if (name.right(4).startsWith('_'))
    {
        bool ok = false;
        name.right(3).toUInt(&ok);
        if (ok)
            name.chop(4);
    }

    // Return the base name with ".log" ending.
    return name + kExtensionWithSeparator;
}

QString File::getFileName(size_t backupNumber) const
{
    return makeFileName(m_settings.name, backupNumber);
}

bool File::openFile()
{
    if (m_file.is_open())
        return true;

    std::cerr << nx::toString(this).toStdString() + ": " + getFileName().toStdString() + '\n';

    const auto fileNameQString = getFileName();
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

void File::archive(QString fileName, QString archiveName)
{
    auto tmpArchiveName = archiveName;
    tmpArchiveName.replace(kRotateExtensionWithSeparator, kRotateTmpExtensionWithSeparator);

    QFile textLog(fileName);
    if (!textLog.open(QIODevice::ReadOnly))
    {
        std::cerr << nx::toString(this).toStdString() << ": Could not open "
            << fileName.toStdString() << '\n';
        return;
    }

    QFile archiveFile(tmpArchiveName);
    if (!archiveFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        std::cerr << nx::toString(this).toStdString() << ": Could not open "
            << tmpArchiveName.toStdString() << '\n';
        return;
    }

    QuaZip archive(&archiveFile);
    archive.setZip64Enabled(true);
    if (!archive.open(QuaZip::Mode::mdCreate))
    {
        std::cerr << nx::toString(this).toStdString() << ": Could not open "
            << tmpArchiveName.toStdString() << '\n';
        return;
    }

    QuaZipFile zippedLog(&archive);
    if (!zippedLog.open(QIODevice::WriteOnly,
        QuaZipNewInfo(makeBaseFileName(fileName), fileName)))
    {
        std::cerr << nx::toString(this).toStdString() << ": Could not zip file "
            << fileName.toStdString() << '\n';
        return;
    }

    while (!textLog.atEnd() && textLog.error() == QFileDevice::NoError
        && zippedLog.getZipError() == 0)
    {
        const auto buf = textLog.read(kBufferSize);
        zippedLog.write(buf);
    }

    if (textLog.error() != QFileDevice::NoError || zippedLog.getZipError() != 0)
    {
        std::cerr << nx::toString(this).toStdString() << ": Could not zip file "
            << fileName.toStdString() << '\n';
        return;
    }

    textLog.close();
    textLog.remove();

    zippedLog.close();
    archive.close();
    archiveFile.rename(archiveName);
}

void File::rotateIfNeeded(nx::Locker<nx::Mutex>* lock)
{
    const auto currentPosition = (size_t) m_file.tellp();
    if (currentPosition < m_settings.size)
        return;

    m_file.close();
    if (m_settings.count == 0)
    {
        QFile::remove(getFileName());
        return;
    }

    if (!needToArchive(lock))
        return;
    NX_ASSERT(!m_archive.valid());

    // In case m_settings.count was modified while we waited for m_archive.get()
    if (m_settings.count == 0)
    {
        QFile::remove(getFileName());
        return;
    }

    size_t firstFreeNumber = m_settings.count;
    if (QFile::exists(getFileName(m_settings.count)))
    {
        // If all files used up we need to rotate all of them.
        QFile::remove(getFileName(1));
        for (size_t i = 2; i <= m_settings.count; ++i)
            QFile::rename(getFileName(i), getFileName(i - 1));
    }
    else
    {
        // Otherwise just move current file to the first free place.
        firstFreeNumber = 1;
        while (QFile::exists(getFileName(firstFreeNumber)))
            ++firstFreeNumber;
    }

    auto archiveName = getFileName(firstFreeNumber);
    auto tmpName = getFileName(firstFreeNumber).replace(kRotateExtensionWithSeparator,
        kTmpExtensionWithSeparator);
    if (QFile::exists(tmpName))
    {
        std::cerr << nx::toString(this).toStdString() << ": Replacing existing log file "
            << tmpName.toStdString() << '\n';
    }
    QFile::rename(getFileName(), tmpName);
    m_archive = std::async(std::launch::async, &File::archive, this, tmpName, archiveName);
}

void File::archiveLeftOvers(nx::Locker<nx::Mutex>* /*lock*/)
{
    auto stem = m_settings.name.split(QDir::separator()).back();
    if (stem.endsWith(kExtensionWithSeparator))
        stem.chop(strlen(kExtensionWithSeparator));

    const auto dir = QFileInfo(getFileName()).dir();
    if (!dir.exists())
        return;

    for (const auto& name: dir.entryList(QStringList{stem + "_*"
        + kTmpExtensionWithSeparator}, QDir::Files))
    {
        QRegExp rx(stem + "_(\\d+)" + kTmpExtensionWithSeparator);
        if (rx.exactMatch(name))
        {
            const auto rotation = rx.cap(1).toInt();
            const auto archiveName = getFileName(rotation);
            const auto tmpName = getFileName(rotation).replace(kRotateExtensionWithSeparator,
                kTmpExtensionWithSeparator);
            archive(tmpName, archiveName);
        }
    }
}

bool File::needToArchive(nx::Locker<nx::Mutex>* lock)
{
    m_archiveQueue++;
    if (m_archive.valid() && m_archiveQueue == 1)
    {
        nx::Unlocker<nx::Mutex> unlocker(lock);
        m_archive.get();
    }
    m_archiveQueue--;
    return m_archiveQueue == 0;
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
