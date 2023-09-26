// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_writers.h"

#include <iostream>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryFile>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <nx/reflect/enum_string_conversion.h>
#include <nx/utils/scope_guard.h>

#include "to_string.h"

static constexpr int kBufferSize = 64 * 1024;

namespace nx {
namespace utils {
namespace log {

cf::future<cf::unit> AbstractWriter::stopArchivingAsync()
{
    return cf::make_ready_future(cf::unit());
}

void StdOut::write(Level level, const QString& message)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    writeImpl(level, message);
}

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

// For Android and iOS, this method is defined in dedicated files.
void StdOut::writeImpl(Level /*level*/, const QString& message)
{
    std::cerr << message.toStdString() + '\n';
}

#endif // !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

QString toQString(File::Extension ext)
{
    return QString::fromStdString(nx::reflect::enumeration::toString(ext));
}

File::File(Settings settings):
    m_settings(std::move(settings)),
    m_fileInfo(makeFileName(m_settings.name, 0, Extension::log)),
    m_volumeLock(m_fileInfo.path() + "/.lock")
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(m_settings.maxVolumeSizeB >= m_settings.maxFileSizeB);
    rotateLeftovers();
}

File::~File()
{
    if (m_archive.valid())
        m_archive.wait();
}

void File::setSettings(const Settings& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    // The filename should not be changed, too dangerous.
    m_settings.maxVolumeSizeB = settings.maxVolumeSizeB;
    m_settings.maxFileSizeB = settings.maxFileSizeB;
    m_settings.maxFileTimePeriodS = settings.maxFileTimePeriodS;
    NX_ASSERT(m_settings.maxVolumeSizeB >= m_settings.maxFileSizeB);
    m_settings.archivingEnabled = settings.archivingEnabled;
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

QString File::makeFileName(QString fileName, size_t backupNumber, File::Extension ext)
{
    auto removedExtension = fileName;
    nx::reflect::enumeration::visitAllItems<Extension>(
        [&removedExtension](auto&&... items)
        {
            for (auto value: {items.value...})
            {
                if (removedExtension.endsWith(toQString(value)))
                    removedExtension.chop(toQString(value).size());
            }
        });

    if (backupNumber == 0)
    {
        return NX_FMT("%1%2", removedExtension, ext);
    }
    else
    {
        return NX_FMT("%1_%2%3")
            .arg(removedExtension).arg(backupNumber, 3, 10, QLatin1Char('0')).arg(ext);
    }
}

QString File::makeBaseFileName(QString fullPath)
{
    // Remove the path.
    auto name = QFileInfo(fullPath).fileName();

    // Remove the extension if any.
    nx::reflect::enumeration::visitAllItems<Extension>(
        [&name](auto&&... items)
        {
            for (auto value: {items.value...})
            {
                if (name.endsWith(toQString(value)))
                    name.chop(toQString(value).size());
            }
        });

    // Remove the rotation counter if any.
    if (name.right(4).startsWith('_'))
    {
        bool ok = false;
        name.right(3).toUInt(&ok);
        if (ok)
            name.chop(4);
    }

    // Return the base name with ".log" ending.
    return name + toQString(Extension::log);
}

QString File::getFileName(size_t backupNumber) const
{
    if (!m_settings.archivingEnabled || backupNumber == 0)
        return makeFileName(m_settings.name, backupNumber, Extension::log);

    return makeFileName(m_settings.name, backupNumber, Extension::zip);
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
    NX_ASSERT(m_settings.archivingEnabled);
    auto tmpArchiveName = archiveName;
    tmpArchiveName.replace(toQString(Extension::zip), toQString(Extension::zipTmp));

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
            makeBaseFileName(fileName).chopped(toQString(Extension::log).size())
                + "_" + QString::number(timestamp) + toQString(Extension::log),
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

void File::rotateAndStartArchivingIfNeeded()
{
    m_volumeLock.lock();
    auto guard = nx::utils::makeScopeGuard([&]() { m_volumeLock.unlock(); });

    auto dir = m_fileInfo.dir();
    const auto stem = makeBaseFileName(m_settings.name).chopped(toQString(Extension::log).size());
    const auto list = dir.entryList({
        stem + "_*" + toQString(Extension::log),
        stem + "_*" + toQString(Extension::zip)}, QDir::Files, QDir::Name);

    int removedCount = 0;
    for (removedCount = 0; removedCount <= list.size() - kMaxLogRotation; removedCount++)
        dir.remove(list[removedCount]);
    for ( ; totalVolumeSize() >= m_settings.maxVolumeSizeB && removedCount < list.size(); removedCount++)
        dir.remove(list[removedCount]);

    if (totalVolumeSize() >= m_settings.maxVolumeSizeB)
    {
        QFile::remove(getFileName());
        return;
    }

    int firstFreeNumber = 1;
    const auto remaining = dir.entryList({
        stem + "_*" + toQString(Extension::log),
        stem + "_*" + toQString(Extension::zip)}, QDir::Files, QDir::Name);
    for (auto fileName: remaining)
    {
        if (firstFreeNumber > kMaxLogRotation)
        {
            std::cerr << nx::toString(this).toStdString() << ": Too many log files, removing "
                << fileName.toStdString() << '\n';
            dir.remove(fileName);
            continue;
        }

        const auto correctName = makeFileName(stem, firstFreeNumber++,
            fileName.endsWith(toQString(Extension::log))
                ? Extension::log
                : Extension::zip);
        if (fileName != correctName)
            dir.rename(fileName, correctName);
    }

    if (!m_settings.archivingEnabled)
    {
        auto rotateName = makeFileName(m_settings.name, firstFreeNumber, Extension::log);
        if (!NX_ASSERT(!QFile::exists(rotateName)))
        {
            std::cerr << nx::toString(this).toStdString() << ": Replacing existing log file "
                << rotateName.toStdString() << '\n';
        }
        QFile::rename(getFileName(), rotateName);
        return;
    }

    auto archiveName = makeFileName(m_settings.name, firstFreeNumber, Extension::zip);
    auto tmpName = makeFileName(m_settings.name, firstFreeNumber, Extension::tmp);
    if (!NX_ASSERT(!QFile::exists(archiveName)))
    {
        std::cerr << nx::toString(this).toStdString() << ": Replacing existing archived log file "
            << archiveName.toStdString() << '\n';
    }
    QFile::rename(getFileName(), tmpName);
    try
    {
        m_archive = std::async(std::launch::async, &File::archive, this, tmpName, archiveName);
    }
    catch (const std::exception& e)
    {
        NX_CRITICAL(this, "Unable to start zipping thread '%1' into '%2': %3",
            tmpName, archiveName, e.what());
    }
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

    rotateAndStartArchivingIfNeeded();
}

void File::rotateLeftovers()
{
    const auto stem = makeBaseFileName(m_settings.name).chopped(toQString(Extension::log).size());
    auto dir = m_fileInfo.dir();
    if (!dir.exists())
        return;

    const auto brokenZipList = dir.entryList({
        stem + "_*" + toQString(Extension::zipTmp)}, QDir::Files, QDir::Name);
    for (auto fileName: brokenZipList)
    {
        std::cerr << nx::toString(this).toStdString() << ": Found log file in unknown state, removing "
            << fileName.toStdString() << '\n';
        dir.remove(fileName);
    }

    int rotation = 1;
    const auto tmpList = dir.entryList({stem + "_*" + toQString(Extension::tmp)}, QDir::Files, QDir::Name);
    for (auto fileName: tmpList)
    {
        if (rotation > kMaxLogRotation)
        {
            std::cerr << nx::toString(this).toStdString() << ": Too many log files, removing "
                << fileName.toStdString() << '\n';
            dir.remove(fileName);
        }
        else
        {
            const auto correctName = QFileInfo(makeFileName(m_settings.name, rotation++, Extension::tmp))
                .fileName();
            if (fileName != correctName)
                dir.rename(fileName, correctName);
        }
    }

    const auto logList = dir.entryList({
        stem + "_*" + toQString(Extension::log),
        stem + "_*" + toQString(Extension::zip)}, QDir::Files, QDir::Name);
    for (auto fileName: logList)
    {
        if (rotation > kMaxLogRotation)
        {
            std::cerr << nx::toString(this).toStdString() << ": Too many log files, removing "
                << fileName.toStdString() << '\n';
            dir.remove(fileName);
        }
        else
        {
            const auto correctName = QFileInfo(makeFileName(
                m_settings.name,
                rotation++,
                fileName.endsWith(toQString(Extension::log)) ? Extension::tmp : Extension::zipTmp))
                .fileName();
            if (fileName != correctName)
                dir.rename(fileName, correctName);
        }
    }

    const auto rotatedList = dir.entryList({
        stem + "_*" + toQString(Extension::tmp),
        stem + "_*" + toQString(Extension::zipTmp)}, QDir::Files, QDir::Name);
    for (auto fileName: rotatedList)
    {
        if (!m_settings.archivingEnabled || fileName.endsWith(toQString(Extension::zipTmp)))
        {
            const auto correctName = fileName.endsWith(toQString(Extension::tmp))
                ? fileName.chopped(toQString(Extension::tmp).size()) + toQString(Extension::log)
                : fileName.chopped(toQString(Extension::zipTmp).size()) + toQString(Extension::zip);
            dir.rename(fileName, correctName);
            continue;
        }

        const auto fullPath = dir.filePath(fileName);
        const auto archivePath = fullPath.chopped(toQString(Extension::tmp).size()) + toQString(Extension::zip);
        if (QFile::exists(archivePath))
        {
            std::cerr << nx::toString(this).toStdString() << ": Replacing existing archived log file "
                << archivePath.toStdString() << '\n';
        }
        archive(fullPath, archivePath);
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

cf::future<cf::unit> File::stopArchivingAsync()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_archiveQueue++;

    if (!m_archive.valid())
        return cf::make_ready_future(cf::unit());

    return cf::initiate(
        [this]() mutable
        {
            try
            {
                m_archive.wait();
                return cf::unit();
            }
            catch (const std::future_error& e)
            {
                return cf::unit();
            }
        });
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
            QString("*") + toQString(Extension::log),
            QString("*") + toQString(Extension::tmp),
            QString("*") + toQString(Extension::zip)
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
