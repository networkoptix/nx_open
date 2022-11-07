// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_writers.h"

#include <iostream>

#include <QtCore/QDateTime>
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
    m_settings(std::move(settings)),
    m_fileInfo(makeFileName(m_settings.name, 0)),
    m_volumeLock(m_fileInfo.path() + "/.lock")
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(m_settings.maxVolumeSizeB >= m_settings.maxFileSizeB);
    archiveLeftOvers(&lock);
}

void File::setSettings(const Settings& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    // The filename should not be changed, too dangerous.
    m_settings.maxVolumeSizeB = settings.maxVolumeSizeB;
    m_settings.maxFileSizeB = settings.maxFileSizeB;
    m_settings.maxFileTimePeriodS = settings.maxFileTimePeriodS;
    NX_ASSERT(m_settings.maxVolumeSizeB >= m_settings.maxFileSizeB);
}

void File::write(Level /*level*/, const QString& message)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!openFile())
    {
        std::cerr << message.toStdString() + '\n';
        return;
    }

    const auto out = message.toStdString();
    m_file << out << std::endl;
    m_currentPos += out.size() + 1;
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
    // Remove the path.
    auto name = QFileInfo(fullPath).fileName();

    // Remove the rotation counter if any.
    if (name.endsWith(kRotateExtensionWithSeparator) ||
        name.endsWith(kRotateTmpExtensionWithSeparator) ||
        name.endsWith(kTmpExtensionWithSeparator))
    {
        if (name.endsWith(kRotateExtensionWithSeparator))
            name.chop(strlen(kRotateExtensionWithSeparator));
        else if (name.endsWith(kRotateTmpExtensionWithSeparator))
            name.chop(strlen(kRotateTmpExtensionWithSeparator));
        else if (name.endsWith(kTmpExtensionWithSeparator))
            name.chop(strlen(kTmpExtensionWithSeparator));

        if (name.right(4).startsWith('_'))
        {
            bool ok = false;
            name.right(3).toUInt(&ok);
            if (ok)
                name.chop(4);
        }
    }
    else if (name.endsWith(kExtensionWithSeparator))
    {
        return name;
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
        m_fileOpenTime = QDateTime::currentDateTime();
        m_currentPos = m_file.tellp();
        return !m_file.fail();
    }

    const auto directory = m_fileInfo.absoluteDir();
    if (!directory.exists())
        directory.mkpath(QLatin1String("."));

    m_file.open(fileName, std::ios_base::out);
    if (m_file.fail())
        return false;
    m_fileInfo.refresh();
    m_fileOpenTime = QDateTime::currentDateTime();

    // Ensure 1st char is UTF8 BOM.
    m_file.write("\xEF\xBB\xBF", 3);
    m_currentPos = 3;
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

    const auto timestamp = QDateTime::currentMSecsSinceEpoch();

    QuaZipFile zippedLog(&archive);
    if (!zippedLog.open(QIODevice::WriteOnly,
        QuaZipNewInfo(
            makeBaseFileName(fileName).chopped(strlen(kExtensionWithSeparator))
                + "_" + timestamp + kExtensionWithSeparator,
            fileName)))
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
    if (!isCurrentLimitReached(lock))
        return;

    m_file.close();

    if (!queueToArchive(lock))
        return;
    NX_ASSERT(!m_archive.valid());

    if (!isCurrentLimitReached(lock))
        return;

    m_volumeLock.lock();
    auto dir = m_fileInfo.dir();
    const auto pattern = QFileInfo(getFileName()).fileName().chopped(strlen(kExtensionWithSeparator))
        + "_*" + kRotateExtensionWithSeparator;
    const auto list = dir.entryList({pattern}, QDir::Files, QDir::Name);

    int removedCount = 0;
    for (removedCount = 0; removedCount <= list.size() - kMaxLogRotation; removedCount++)
        dir.remove(list[removedCount]);
    for ( ; totalVolumeSize() >= m_settings.maxVolumeSizeB && removedCount < list.size(); removedCount++)
        dir.remove(list[removedCount]);

    if (totalVolumeSize() >= m_settings.maxVolumeSizeB)
    {
        QFile::remove(getFileName());
        m_volumeLock.unlock();
        return;
    }

    int firstFreeNumber = 1;
    const auto remaining = dir.entryList({pattern}, QDir::Files, QDir::Name);
    for (auto fileName: remaining)
    {
        const auto correctName = QFileInfo(getFileName(firstFreeNumber++)).fileName();
        if (fileName != correctName)
            dir.rename(fileName, correctName);
    }
    NX_ASSERT(firstFreeNumber <= kMaxLogRotation);

    auto archiveName = getFileName(firstFreeNumber);
    auto tmpName = getFileName(firstFreeNumber).replace(kRotateExtensionWithSeparator,
        kTmpExtensionWithSeparator);
    NX_ASSERT(!QFile::exists(archiveName));
    if (QFile::exists(tmpName))
    {
        std::cerr << nx::toString(this).toStdString() << ": Replacing existing log file "
            << tmpName.toStdString() << '\n';
    }
    QFile::rename(getFileName(), tmpName);
    m_archive = std::async(std::launch::async, &File::archive, this, tmpName, archiveName);
    m_volumeLock.unlock();
}

void File::archiveLeftOvers(nx::Locker<nx::Mutex>* /*lock*/)
{
    const auto stem = QFileInfo(getFileName()).fileName().chopped(strlen(kExtensionWithSeparator));
    const auto dir = m_fileInfo.dir();
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
            if (QFile::exists(archiveName))
            {
                std::cerr << nx::toString(this).toStdString() << ": Replacing existing archive file "
                    << archiveName.toStdString() << '\n';
            }
            archive(tmpName, archiveName);
        }
    }
}

bool File::queueToArchive(nx::Locker<nx::Mutex>* lock)
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

bool File::isCurrentLimitReached(nx::Locker<nx::Mutex>* /*lock*/)
{
    if (m_currentPos >= m_settings.maxFileSizeB)
        return true;

    if (m_settings.maxFileTimePeriodS != std::chrono::seconds::zero())
    {
        const auto now = QDateTime::currentDateTime();
        const auto timeLimit = m_fileOpenTime.addSecs(m_settings.maxFileTimePeriodS.count());

        if (timeLimit.isValid() && now >= timeLimit)
            return true;
    }

    return false;
}

qint64 File::totalVolumeSize()
{
    qint64 size = 0;

    m_fileInfo.refresh();
    auto dir = m_fileInfo.dir();

    for (auto fileInfo: dir.entryInfoList(
        {
            QString("*") + kExtensionWithSeparator,
            QString("*") + kTmpExtensionWithSeparator,
            QString("*") + kRotateExtensionWithSeparator
        },
        QDir::Files))
    {
        size += fileInfo.size();
    }

    return size;
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
