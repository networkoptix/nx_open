// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_writers.h"

#include <cstdio>
#include <iostream>
#include <optional>
#include <set>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryFile>

#if defined(Q_OS_WIN)
    #include <QtCore/qt_windows.h>
#endif

#include <nx/reflect/enum_string_conversion.h>
#include <nx/utils/nx_utils_ini.h>
#include <nx/utils/scope_guard.h>

#include "storage_info.h"
#include "to_string.h"

namespace {

constexpr int kBufferSize = 64 * 1024;

bool renameFileWithoutCopyFallback(const QString& sourcePath, const QString& targetPath)
{
    if (QFile::exists(targetPath))
        return false;

#if defined(Q_OS_WIN)
    const auto nativeSourcePath = QDir::toNativeSeparators(sourcePath);
    const auto nativeTargetPath = QDir::toNativeSeparators(targetPath);
    return MoveFileW(reinterpret_cast<LPCWSTR>(nativeSourcePath.utf16()),
        reinterpret_cast<LPCWSTR>(nativeTargetPath.utf16()));
#else
    const auto encodedSourcePath = QFile::encodeName(sourcePath);
    const auto encodedTargetPath = QFile::encodeName(targetPath);
    return std::rename(encodedSourcePath.constData(), encodedTargetPath.constData()) == 0;
#endif
}

constexpr int kRotationNumberDigits = 3;

/** @return Rotation number of a file name. */
std::optional<int> rotationNumber(const QString& name)
{
    if (name.size() <= kRotationNumberDigits
        || name[name.size() - kRotationNumberDigits - 1] != QChar('_'))
    {
        return std::nullopt;
    }

    bool ok = false;
    const int number = name.right(kRotationNumberDigits).toUInt(&ok);
    return ok ? std::optional<int>(number) : std::nullopt;
}

} // namespace

namespace nx::log {

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
    NX_ASSERT(settings.maxVolumeSizeB >= settings.maxFileSizeB);

    NX_MUTEX_LOCKER lock(&m_mutex);
    // The filename should not be changed, too dangerous.
    m_settings.maxVolumeSizeB = settings.maxVolumeSizeB;
    m_settings.maxFileSizeB = settings.maxFileSizeB;
    m_settings.maxFileTimePeriodS = settings.maxFileTimePeriodS;
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
            .arg(removedExtension)
            .arg(backupNumber, kRotationNumberDigits, 10, QLatin1Char('0'))
            .arg(ext);
    }
}

QString File::makeBaseFileName(QString fullPath, bool removeRotationCounter)
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

    if (removeRotationCounter && rotationNumber(name))
        name.chop(kRotationNumberDigits + 1);

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

    m_file.open(fileName, std::ios::in | std::ios::out | std::ios::binary);
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

    m_file.open(fileName, std::ios::out | std::ios::binary);
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

    zippedLog.close();
    archive.close();
    archiveFile.close();
    if (!renameFileWithoutCopyFallback(tmpArchiveName, archiveName))
    {
        std::cerr << nx::toString(this).toStdString() << ": Could not finalize archived log file "
                  << tmpArchiveName.toStdString() << " as " << archiveName.toStdString() << '\n';
        return;
    }

    textLog.close();
    textLog.remove();
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
    auto existingCount = list.size();
    for (; existingCount >= kMaxLogRotation && removedCount < list.size(); removedCount++)
    {
        if (dir.remove(list[removedCount]))
            --existingCount;
    }

    const qint64 maxVolumeSizeB = calculateMaxVolumeSize();

    for ( ; totalVolumeSize() >= maxVolumeSizeB && removedCount < list.size(); removedCount++)
        dir.remove(list[removedCount]);

    if (totalVolumeSize() >= maxVolumeSizeB)
    {
        QFile::remove(getFileName());
        return;
    }

    std::set<int> keptNumbers;
    int nextNumber = 1;
    const auto freeNumber = [&]()
    {
        while (keptNumbers.contains(nextNumber))
            ++nextNumber;
        return nextNumber;
    };

    const auto remaining = dir.entryList({
        stem + "_*" + toQString(Extension::log),
        stem + "_*" + toQString(Extension::zip)}, QDir::Files, QDir::Name);
    for (const auto& fileName: remaining)
    {
        const auto extension =
            fileName.endsWith(toQString(Extension::log)) ? Extension::log : Extension::zip;
        for (;;)
        {
            const int number = freeNumber();
            if (number > kMaxLogRotation)
            {
                std::cerr << nx::toString(this).toStdString() << ": Too many log files, removing "
                          << fileName.toStdString() << '\n';
                dir.remove(fileName);
                break;
            }

            const auto correctName = makeFileName(stem, number, extension);
            if (fileName == correctName
                || renameFileWithoutCopyFallback(
                    dir.filePath(fileName), dir.filePath(correctName)))
            {
                ++nextNumber;
                break;
            }

            if (QFile::exists(dir.filePath(correctName)))
            {
                // The name is occupied by something else, e.g. a directory.
                ++nextNumber;
                continue;
            }

            std::cerr << nx::toString(this).toStdString() << ": Could not rename log file "
                      << fileName.toStdString() << " to " << correctName.toStdString() << '\n';
            if (const auto kept = rotationNumber(fileName.chopped(toQString(extension).size())))
                keptNumbers.insert(*kept);
            break;
        }
    }

    if (!m_settings.archivingEnabled)
    {
        auto rotateName = makeFileName(m_settings.name, freeNumber(), Extension::log);
        if (!renameFileWithoutCopyFallback(getFileName(), rotateName))
        {
            std::cerr << nx::toString(this).toStdString() << ": Could not rotate log file "
                      << getFileName().toStdString() << " to " << rotateName.toStdString() << '\n';
        }
        return;
    }

    const int number = freeNumber();
    auto archiveName = makeFileName(m_settings.name, number, Extension::zip);
    auto tmpName = makeFileName(m_settings.name, number, Extension::tmp);
    if (!renameFileWithoutCopyFallback(getFileName(), tmpName))
    {
        // A copying fallback must not run here: rotation is synchronous under the log writer lock,
        // so a blocked copy would stop every thread trying to write a log message.
        std::cerr << nx::toString(this).toStdString() << ": Could not rotate log file "
                  << getFileName().toStdString() << " to " << tmpName.toStdString() << '\n';
        return;
    }
    try
    {
        m_archive = std::async(std::launch::async, &File::archive, this, tmpName, archiveName);
    }
    catch (const std::exception& e)
    {
        const QString message = nx::format(
            "Unable to start zipping thread '%1' into '%2': %3", tmpName, archiveName, e.what());
        std::cerr << nx::toString(this).toStdString() << ": " << message.toStdString() << '\n';
        nx::crashProgram(message);
    }
}

void File::rotateIfNeeded(nx::Locker<nx::Mutex>* lock)
{
    if (!isCurrentLimitReached(lock))
        return;

    m_file.close();

    if (!queueToArchive(lock))
        return;
    if (m_archive.valid())
    {
        std::cerr << nx::toString(this).toStdString()
                  << ": Previous log archiving has not been finished\n";
    }

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

    std::set<int> keptNumbers;
    int rotation = 1;
    const auto nextRotation = [&]()
    {
        while (keptNumbers.contains(rotation))
            ++rotation;
        return rotation;
    };
    const auto renameOrKeepNumber =
        [&](const QString& fileName, Extension extension, const QString& correctName)
    {
        if (fileName == correctName
            || renameFileWithoutCopyFallback(dir.filePath(fileName), dir.filePath(correctName)))
        {
            return;
        }

        std::cerr << nx::toString(this).toStdString() << ": Could not rename log file "
                  << fileName.toStdString() << " to " << correctName.toStdString() << '\n';
        if (const auto kept = rotationNumber(fileName.chopped(toQString(extension).size())))
            keptNumbers.insert(*kept);
    };

    const auto tmpList = dir.entryList({stem + "_*" + toQString(Extension::tmp)}, QDir::Files, QDir::Name);
    for (auto fileName: tmpList)
    {
        if (nextRotation() > kMaxLogRotation)
        {
            std::cerr << nx::toString(this).toStdString() << ": Too many log files, removing "
                << fileName.toStdString() << '\n';
            dir.remove(fileName);
        }
        else
        {
            const auto correctName = QFileInfo(makeFileName(m_settings.name, rotation++, Extension::tmp))
                .fileName();
            renameOrKeepNumber(fileName, Extension::tmp, correctName);
        }
    }

    const auto logList = dir.entryList({
        stem + "_*" + toQString(Extension::log),
        stem + "_*" + toQString(Extension::zip)}, QDir::Files, QDir::Name);
    for (auto fileName: logList)
    {
        const auto extension =
            fileName.endsWith(toQString(Extension::log)) ? Extension::log : Extension::zip;
        if (nextRotation() > kMaxLogRotation)
        {
            std::cerr << nx::toString(this).toStdString() << ": Too many log files, removing "
                << fileName.toStdString() << '\n';
            dir.remove(fileName);
        }
        // Is indexed log, non unarchived with timestamp or current one
        else if (extension == Extension::log
            && !rotationNumber(fileName.chopped(toQString(extension).size())))
        {
            // do nothing with such files
            continue;
        }
        else
        {
            const auto correctName =
                QFileInfo(makeFileName(m_settings.name,
                              rotation++,
                              extension == Extension::log ? Extension::tmp : Extension::zipTmp))
                    .fileName();
            renameOrKeepNumber(fileName, extension, correctName);
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
            renameFileWithoutCopyFallback(dir.filePath(fileName), dir.filePath(correctName));
            continue;
        }

        const auto fullPath = dir.filePath(fileName);
        const auto archivePath = fullPath.chopped(toQString(Extension::tmp).size()) + toQString(Extension::zip);
        if (QFile::exists(archivePath))
        {
            // The log file stays for the next start, the existing archive is not replaced.
            std::cerr << nx::toString(this).toStdString() << ": Could not archive log file "
                      << fileName.toStdString() << ": " << archivePath.toStdString()
                      << " already exists\n";
            continue;
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
            catch (const std::future_error&)
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

qint64 File::bytesToRemoveByFreeSpaceLimit() const
{
    qint64 requiredSize = nx::utils::ini().logReservedVolumeSizeBytes();
    int requiredPercent = std::clamp(nx::utils::ini().logReservedVolumePercentage, 0, 100);

    // Avoid calling expensive getVolumeInfo if checks are disabled.
    if (requiredSize <= 0 && requiredPercent <= 0)
        return 0;

    auto volumeInfoOpt = nx::log::detail::getVolumeInfo(m_fileInfo.dir().path());
    if (!volumeInfoOpt)
        return 0;

    const auto [totalB, freeB] = *volumeInfoOpt;
    qint64 requiredPercentSize = totalB * requiredPercent / 100;

    qint64 reserveB = requiredPercentSize > 0 && requiredSize > 0
            ? std::min(requiredSize, requiredPercentSize)
            : std::max(requiredSize, requiredPercentSize);
    return freeB > reserveB ? 0 : reserveB - freeB;
}

qint64 File::calculateMaxVolumeSize()
{
    if (auto bytesToRemove = bytesToRemoveByFreeSpaceLimit(); bytesToRemove > 0)
    {
        if (auto logsTotalSize = totalVolumeSize(); logsTotalSize >= bytesToRemove)
            return std::min(m_settings.maxVolumeSizeB, logsTotalSize - bytesToRemove);
        else
            return 0; //There is not enough space even after we remove all logs.
    }
    return m_settings.maxVolumeSizeB;
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

} // namespace nx::log
